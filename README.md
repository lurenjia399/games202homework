



# games202homework

games202的作业

# 1 阴影部分

## 1 shadowmap

原理：两趟pass，第一趟在光源位置放置摄像机，渲染出场景并将其深度保存起来，作为贴图也就是shadowmap。第二趟正常渲染，将可见点投影回第一趟的空间，然后得到深度h，将其和shadowmap中记录的深度shadowdepth进行比较，h < shadowmap 可见，否则不可见也就是在阴影中。

shader:

```glsl
float useShadowMap(sampler2D shadowMap, vec4 shadowCoord){
  // 这里就是通过uv坐标对shadowmap进行采样，然后通过unpack将采出的颜色值转换为深度
  float shadowMapDepth = unpack(texture2D(shadowMap, shadowCoord.xy));
  // 1 shaderpoint的深度 > shadowMapDepth 在阴影中
  // 2 这里加了个bias，这个后面解释下
  if(shadowCoord.z - BIAS > shadowMapDepth){
    return 0.0;
  }else
  {
    return 1.0;
  }
}
void main(void) {

  float visibility;
    
  // 1 vPositionFromLight个值是在顶点着色器中计算，将顶点 * mvp矩阵得到，这里的mvp矩阵是第一趟pass中的
  // 2 经过mvp矩阵后得到的是其次剪裁空间，也就是[-1,1]的立方体中，因为这里是正交投影所以也不用透视除法了，w分量一直为1
  // 3 因为vPositionFromLight得到的范围是[-1,1]，而贴图中uv坐标范围是[0,1]，深度坐标范围[0,1]，所以需要进行 x * 0.5 + 0.5 的转化
  vec3 shadowCoord = vPositionFromLight.xyz * 0.5 +vec3(0.5, 0.5, 0.5);

  visibility = useShadowMap(uShadowMap, vec4(shadowCoord, 1.0));

  vec3 phongColor = blinnPhong();

  gl_FragColor = vec4(phongColor * visibility, 1.0);
}
```

注意到在比较深度的时候加了一个偏移，这是为什么呢？

shadow map方法的一个缺点在于其阴影质量取决于shadow map的分辨率和z-buffer的数值精度，所以存在一个常见的自阴影走样（self-shadow aliasing）问题，将会导致一些三角形被误认为是阴影，如下图的地面。

![image-20230919203927151](D:\GitHub\games202homework\image\image-20230919203927151.png)

这个错误源于两点，其一是处理器数值精度不够，其二是实际上z-buffer中采样的像素点包含了一片区域，计算的仅是这片区域中心的位置。如下图中，视野看向的位置被错误地遮挡了。

![image-20230919204003809](D:\GitHub\games202homework\image\image-20230919204003809.png)

由于shadowmap的精度不够的时候，一个像素对应一个深度，一个像素又会对应许多个采样点，也就是有不同的采样点对应了相同的深度也就是图中红色所指的，而第二趟的时候shaderPoint的采样点正在被前面挡住，导致了地面上的这中遮挡。解决方式就是添加个偏移，上shaderPoint的深度变浅，尽量放置被自遮挡。

这个偏移还不能太大，太大的话就会漏光，就像这样，

![image-20230919205225150](D:\GitHub\games202homework\image\image-20230919205225150.png)

对于这种问题的解决方法是一种被称为第二深度阴影映射（second-depth shadow mapping ）的方法，这种方法能很好的解决许多无法通过手动调整的漏光问题，即用特殊的shadow map来计算遮挡，如下图的三种shadow map计算方法，需要根据实际情况来决定使用哪一种，且都要求物体必须是闭合的。

![image-20230919205357707](D:\GitHub\games202homework\image\image-20230919205357707.png)

效果：

![image-20230921172506250](D:\GitHub\games202homework\image\image-20230921172506250.png)



## 2 PCF

原理：

区别于硬阴影，其总体思路为对采样点周围的一片区域进行采样，计算顶点被这些采样点遮挡的总体数量，再除以采样点的数量，作为该点被遮挡的程度系数。因此，它不是一个非0即1的数，而是一个浮点数。

shader：

```glsl
float PCF(sampler2D shadowMap, vec4 coords) 
{
  poissonDiskSamples(coords.xy);
  float sum = 0.0;
  for(int i = 0; i < NUM_SAMPLES; i ++)
  {
    // 得到了随机采样点poissonDisk[i]
    // RADIUS / 2048.0 这个的目的是控制采样的范围，2048是指贴图的大小
    vec2 xy = poissonDisk[i] * RADIUS / 2048.0  + coords.xy;
    float shadowMapDepth = unpack(texture2D(shadowMap, xy));
    if(coords.z > shadowMapDepth + BIAS)
    {
      // 在阴影里面
      sum = sum + 1.0;
    }
  }
  return 1.0 - sum / float( NUM_SAMPLES );
}
```

![image-20230921172534051](D:\GitHub\games202homework\image\image-20230921172534051.png)



## 3 PCSS

### avgblocker depth

其中， findBlocker 函数中需要我们完成对遮挡物平均深度的计算。首先，还是调用采样函数，生成一系列的采样点。然后遍历数组，对于每一个偏移的 uv 坐标采样到的深度，若其产生了遮挡，则进行计数。最终计算得到被遮挡的平均深度。

这里需要注意的是要考虑没有遮挡的情况，即计数为0，此时在计算遮挡的平均深度时会发生除数为0的情况。shader里面情况一有体现。

同时，uv 偏移半径的选择对最终的结果也非常重要。由于远处的物体更容易被遮挡，当选择固定的较小的 radius 值时，其对距离光源较远的顶点进行平均遮挡深度的计算可能无法很好的表现出其被遮挡的程度（因为光源实际上是有一定形状的，而不是理想的无体积点，距离光源越远，光源的不同部分就越有可能被遮挡，从而无法对该点的照明做出贡献）。

在采样数较多的情况下，这种缺陷会被放大，较小的 radius 会导致距离光源较远处的阴影边缘处的过渡很生硬，这正是因为较小的采样半径使得距离光源较远处的顶点采样时的平均遮挡深度过渡很生硬（可以类比模糊操作，使用较小的卷积核对边缘锐利处的模糊作用并不明显）

这种剧烈的平均遮挡深度的变化会导致计算的半影直径的剧烈变化，从而导致最后的 PCF 采样半径在边缘处的突变，从而使边缘处的阴影过渡生硬。

然而，过大的 radius 同样存在问题，即计算出的平均遮挡深度无法准确表示该点的被遮挡情况，顶点周围的平均遮挡深度相似，从而导致计算出的半影直径相似，无法表现出距离越远阴影越模糊的效果。

因此，我们考虑使用自适应的采样半径 radius，顶点距离光源越远，其在 Shadowmap 上的成像范围就越大，应使用更大的采样半径，反之则使用更小的采样半径，如下图所示：

![image-20230921171356908](D:\GitHub\games202homework\image\image-20230921171356908.png)



### penumbra size

根据前面计算出来的平均遮挡深度，我们可以利用相似三角形的性质计算出半影直径：

![image-20230921171951279](D:\GitHub\games202homework\image\image-20230921171951279.png)

shader:

```glsl
#define BIAS 0.0002			// 处理自遮挡问题的bias
#define RADIUS 15.0			// pcf的采样范围
#define NEAR_PLANE 0.0001	// 近平面的距离
#define LIGHT_UV_SIZE 25.5 	// 光源范围
    
float findBlocker( sampler2D shadowMap,  vec2 uv, float zReceiver ) {
  // 这里是用相似三角形来确定图一中红色区域的范围
  float radiusScale = LIGHT_UV_SIZE * (zReceiver - NEAR_PLANE) / zReceiver;
  poissonDiskSamples(uv);
  float totalShadowDepth = 0.0;
  int totalShadowNum = 0;
  for(int i = 0; i < NUM_SAMPLES; i++)
  {
    vec2 DiskUV = poissonDisk[i] * radiusScale / 2048.0 + uv;
    float shadowDepth = unpack(texture2D(shadowMap, DiskUV));
    if(zReceiver > shadowDepth + BIAS)
    {
      // 在阴影里面
      totalShadowDepth += shadowDepth;
      totalShadowNum ++;
    }
  }
  // 情况一：这两个判断很重要，防止除0的情况
  if(totalShadowNum == 0)
  {
    return 1.0;
  }
  if(totalShadowNum == NUM_SAMPLES)
  {
    return 0.0;
  }
	return totalShadowDepth / float(totalShadowNum);
}

float PCSS(sampler2D shadowMap, vec4 coords){

  // STEP 1: avgblocker depth
  float avgblockerdepth = findBlocker(shadowMap, coords.xy, coords.z);

  // STEP 2: penumbra size
  // 第二步就是图二上面的，用相似三角形算出了w，第一步算出的avgblockerdepth就是绿色的部分
  float penumbrasize = (coords.z - avgblockerdepth) / avgblockerdepth * LIGHT_UV_SIZE;

  // STEP 3: filtering
   float result = PCF(shadowMap, coords, penumbrasize);
  
  return result;

}
```

效果图：也算凑乎吧

![image-20230921170326017](D:\GitHub\games202homework\image\image-20230921170326017.png)
