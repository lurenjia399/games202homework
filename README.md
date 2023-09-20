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

## 2 PCF

原理：

shader：

```glsl
float PCF(sampler2D shadowMap, vec4 coords) 
{

  poissonDiskSamples(coords.xy);
  float sum = 0.0;
  for(int i = 0; i < NUM_SAMPLES; i ++)
  {
    // 每次单位圆上的采样点，这边 / 2048是因为shadowmap的大小是2048 * 2048
    vec2 xy = (poissonDisk[i] * RADIUS) / 2048.0  + coords.xy;
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

