



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

![image-20230919203927151](F:\learn\games202homework\image\image-20230919203927151.png)

这个错误源于两点，其一是处理器数值精度不够，其二是实际上z-buffer中采样的像素点包含了一片区域，计算的仅是这片区域中心的位置。如下图中，视野看向的位置被错误地遮挡了。

![image-20230919204003809](F:\learn\games202homework\image\image-20230919204003809.png)

由于shadowmap的精度不够的时候，一个像素对应一个深度，一个像素又会对应许多个采样点，也就是有不同的采样点对应了相同的深度也就是图中红色所指的，而第二趟的时候shaderPoint的采样点正在被前面挡住，导致了地面上的这中遮挡。解决方式就是添加个偏移，上shaderPoint的深度变浅，尽量放置被自遮挡。

这个偏移还不能太大，太大的话就会漏光，就像这样，

![image-20230919205225150](F:\learn\games202homework\image\image-20230919205225150.png)

对于这种问题的解决方法是一种被称为第二深度阴影映射（second-depth shadow mapping ）的方法，这种方法能很好的解决许多无法通过手动调整的漏光问题，即用特殊的shadow map来计算遮挡，如下图的三种shadow map计算方法，需要根据实际情况来决定使用哪一种，且都要求物体必须是闭合的。

![image-20230919205357707](F:\learn\games202homework\image\image-20230919205357707.png)

效果：

![image-20230921172506250](F:\learn\games202homework\image\image-20230921172506250.png)



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

![image-20230921172534051](F:\learn\games202homework\image\image-20230921172534051.png)



## 3 PCSS

### avgblocker depth

其中， findBlocker 函数中需要我们完成对遮挡物平均深度的计算。首先，还是调用采样函数，生成一系列的采样点。然后遍历数组，对于每一个偏移的 uv 坐标采样到的深度，若其产生了遮挡，则进行计数。最终计算得到被遮挡的平均深度。

这里需要注意的是要考虑没有遮挡的情况，即计数为0，此时在计算遮挡的平均深度时会发生除数为0的情况。shader里面情况一有体现。

同时，uv 偏移半径的选择对最终的结果也非常重要。由于远处的物体更容易被遮挡，当选择固定的较小的 radius 值时，其对距离光源较远的顶点进行平均遮挡深度的计算可能无法很好的表现出其被遮挡的程度（因为光源实际上是有一定形状的，而不是理想的无体积点，距离光源越远，光源的不同部分就越有可能被遮挡，从而无法对该点的照明做出贡献）。

在采样数较多的情况下，这种缺陷会被放大，较小的 radius 会导致距离光源较远处的阴影边缘处的过渡很生硬，这正是因为较小的采样半径使得距离光源较远处的顶点采样时的平均遮挡深度过渡很生硬（可以类比模糊操作，使用较小的卷积核对边缘锐利处的模糊作用并不明显）

这种剧烈的平均遮挡深度的变化会导致计算的半影直径的剧烈变化，从而导致最后的 PCF 采样半径在边缘处的突变，从而使边缘处的阴影过渡生硬。

然而，过大的 radius 同样存在问题，即计算出的平均遮挡深度无法准确表示该点的被遮挡情况，顶点周围的平均遮挡深度相似，从而导致计算出的半影直径相似，无法表现出距离越远阴影越模糊的效果。

因此，我们考虑使用自适应的采样半径 radius，顶点距离光源越远，其在 Shadowmap 上的成像范围就越大，应使用更大的采样半径，反之则使用更小的采样半径，如下图所示：

![image-20230921171356908](F:\learn\games202homework\image\image-20230921171356908.png)



### penumbra size

根据前面计算出来的平均遮挡深度，我们可以利用相似三角形的性质计算出半影直径：

![image-20230921171951279](F:\learn\games202homework\image\image-20230921171951279.png)

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

![image-20230921170326017](F:\learn\games202homework\image\image-20230921170326017.png)

#  2 Image base lighting(LBL)

shading from Environment Lighting

## 1 Split Sum(分布求和)

![](F:\learn\games202homework\image\image-20231113222940203.png)

如何得到每个采样点的颜色呢？就需要解渲染方程

1 光源项：方程中的光源项就是环境贴图中记录的rgb颜色，但是会有问题（1 在shader中进行samples就会很慢，每个采样点都需要进行计算。2 解渲染方程可以通过蒙特卡洛积分的方式，那也需要大量的数据才行）基于以上的问题所以我们希望不进行采样就能获取到光源项。

![image-20231113223640073](F:\learn\games202homework\image\image-20231113223640073.png)

2 BRDF项：通过观察，当BRDF光滑的时候，反射的W0范围比较小

当FBRDF粗糙的时候，反射就在半球上，比较均匀，也就是平滑。

根据以上两个观察就能将光源项提出来，并单位化

![image-20231113224326923](F:\learn\games202homework\image\image-20231113224326923.png)

### 1 那么黄色框这部分代表什么呢？光源的颜色 / 一个范围

![image-20231113225309226](F:\learn\games202homework\image\image-20231113225309226.png)

代表的就是一个滤波，也就是获取到当前像素点的颜色，然后除以一小块范围

，在写回这个像素点。通俗点就是一个模糊操作。

（注意这可以预计算也就是提前算好，然后再渲染的过程中使用。预计算根据滤波核的大小生成不同的图片，在shader中根据BRDF的W0的大小确定滤波核，然后就能通过双线性插值类似mipmap的方式采样图中的颜色）。

![image-20231113225546658](F:\learn\games202homework\image\image-20231113225546658.png)

根据这幅图，我们就可以通过r的方向来查询预计算出的结果来得到出射光的radiance。

### 2 下面看剩下的部分，一个BRDF积分

![image-20231113230306065](F:\learn\games202homework\image\image-20231113230306065.png)

我们也可以采用预计算的方式，也就是通过遍历所有的参数来获得BRDF项

在微表面模型下，我们的BRDF为

![image-20231113231141766](F:\learn\games202homework\image\image-20231113231141766.png)

其中：F() D() 分别为

![image-20231113231157824](F:\learn\games202homework\image\image-20231113231157824.png)

G()为

![image-20231113231637007](F:\learn\games202homework\image\image-20231113231637007.png)

也就是三个参数组成，但是我们觉得参数还是太多了，还想要降维

![image-20231113232213927](F:\learn\games202homework\image\image-20231113232213927.png)

通过菲涅尔的公式F项，这边进行一个纯数学的展开，将R0基础反射率提出来，这里手动在纸上算下就行。

至此，我们就将BRDF项中参数降到两维了，一个式roughness,一个是角度。也就是可以与计算出一张图了。

![image-20231113232650802](F:\learn\games202homework\image\image-20231113232650802.png)

### 3 结果：

![image-20231113232853490](F:\learn\games202homework\image\image-20231113232853490.png)

![image-20231113232943590](F:\learn\games202homework\image\image-20231113232943590.png)

# 3 Precompute Radiance Transfer(PRT)

## 1 前导知识

### 1 卷积

![image-20231115103439390](F:\learn\games202homework\image\image-20231115103439390.png)

时域上模糊的操作就是频域上的乘积，也就是卷积。

### 2 基函数

![image-20231115104032143](F:\learn\games202homework\image\image-20231115104032143.png)

基函数的表示

### 3 球谐函数

![image-20231115104525709](F:\learn\games202homework\image\image-20231115104525709.png)

任何一个二维函数都能用球谐函数表示，类似于一维里面的傅里叶展开

![image-20231115105601835](F:\learn\games202homework\image\image-20231115105601835.png)

任何一个二维函数 * 任意一个球谐函数的基函数 在积分 就能得到对应基函数的系数，这个过程也叫投影。

类比于一个向量，他就能用三个基向量的系数来表示。

两个函数相乘求积分被称为函数内积，这个操作就相当于求两个向量内积，也就是点乘。

## 2 PRT

![image-20231115113929225](F:\learn\games202homework\image\image-20231115113929225.png)

渲染方程中的三个积分项都可以看作是球面函数 

![image-20231115114720356](F:\learn\games202homework\image\image-20231115114720356.png)

作出假设，场景中的所有东西都不变，将渲染方程就看作两部分。

lighting：将其用基函数的方式表示。

Lighting transport：一个shadering point 上的性质，可以看作是这样，因为场景中所有东西都不变。

然后我们分成情况分析：

### 1 diffuse 

![image-20231115233234921](F:\learn\games202homework\image\image-20231115233234921.png)

1 diffuse的情况下BRDF是一个常数可以提出来

2 lighting项可以通过预计算得到，从每个环境贴图的像素中读取光照颜色，然后将其投影到基函数上得到系数，那么原光源就等于图中黄色的部分，每个系数乘上基函数的和 等于光源

3 然后通过数学变换，就可以看出剩余部分也可以通过预计算，将可见项投影到基函数上得到系数

4 最终的结果就是 两个系数的乘积在求和，别忘了乘上BRDF这个常数

这里再写作业的时候，需要注意的就是我们的这些系数都是RGB三个通道来表示，所以就是在各个通道上我们要求一个系数的乘积和，最后得到Shadering的值

### 2 glossy

glossy 的物体和diffuse的物体有什么区别呢？他们只是在BRDF上有区别，

diffuse物体的BRDF是一个常数也就是无论我们从哪个方向看采样点的颜色都是一样的，也就是个二维的函数，只和入射角有关

glossy物体的BRDF就不是一个常数了，我们从不同的方向看过去得到的结果都是不一样的，也就是个四维的函数，和入射角和出射角都有关系

![image-20231116000529604](F:\learn\games202homework\image\image-20231116000529604.png)

第一步：我们通过PRT近似算出了shadering的结果，但是Lt项是一个关于出射角的函数。（可以这样看，我们固定一个出射角，那么LightTransport项都只和入射角有关，也就是可以看成球面函数了，然后通过投影基函数就能得出Lt，所以也就是Lt项是一个关于出射角的函数，每给定一个出射角Lt函数都能得出具体的Lt值）

![image-20231116001101922](F:\learn\games202homework\image\image-20231116001101922.png)

第二步：我们得到了关于o的Lt函数，那么我们就可以对o投影到基函数上，那么就得到了最终的式子。

![image-20231116001304362](F:\learn\games202homework\image\image-20231116001304362.png)

所以也就能看出L项是一个向量，每个小格子代表一个系数，Lt项是一个矩阵，每个小格子也是一个系数，他俩的乘积得到3阶sh函数的9个系数，然后在分别乘上o向上的基函数求和，就能算出最终的shadering。

# 4 Reflective Shadow Maps(RSM)

![image-20231118171014447](F:\learn\games202homework\image\image-20231118171014447-17005787025021.png)

次级光源对p点的贡献，求p点的渲染方程，将立体角转化为对光源的一个积分，就和101中的路径追踪一样。

还记得为什么要转化么？

1 从摄像机开始，向场景中打入一条光线，记录打中点p

2 从p点开始朝向某个方向打出光线，这条光线打中光源，才会被选取作为出射方向

3 在第二步中选择某个方向的时候，如果光源比较小，那我们就需要更多的光线才能打到光源，这样就会造成浪费

4 基于3的问题，我们就直接进行转化，对光源进行采样，这样就不会造成浪费，从光源直接采样出打到p的射线，如图

![image-20231118172012052](F:\learn\games202homework\image\image-20231118172012052-17005787025032.png)

回到RSM，我们看下理论推导：

![image-20231118172317617](F:\learn\games202homework\image\image-20231118172317617-17005787025033.png)

1 立体角w和光源单位面积A的转化，上面说清楚了

2 如果A的面积比较小的化，我们就不用积分了，直接乘delta A就行，黎曼积分

3 对于一个diffuse reflective patch 的情况，这里看向的是次级光源，q点的BRDF 是一个常数，Lo出射光的Radiance（也就是q->p的能量）等于q的BRDF * q点的Irradiance

4 将2和3联立下，就能得出图片中的公式：其中p点的能量就代表q点BRDF * q 点的能量 。其中分子就是两个夹角（注意这两个家教p到q的向量没有单位话，所以分母上多了一个距离的平方）

5 渲染方程中还有visibility项，计算很困难，这里不考虑了

# 5 Screen Space Ambient Occlusion(SSAO)

在屏幕空间下对全局光照的一个近似。

我们不知道间接光照是什么，所以假设每个采样点所接受的间接光照都是一个常数。

![image-20231122104744188](F:\learn\games202homework\image\image-20231122104744188.png)

虽然我们认为间接光照是一个常数，但对于一个采样点来说，它无法接受到半球上所有方向的间接光照，会有遮挡，就像图中的AO贴图那样。

## 1 理论基础是什么呢？

![image-20231122110257825](F:\learn\games202homework\image\image-20231122110257825.png)

我们把visibility拆出来，什么情况下能拆出来呢？（1 g(x)的覆盖范围很小 2 g(x) 是平滑的 ）：

![image-20231122112251871](F:\learn\games202homework\image\image-20231122112251871.png)

蓝色框：就是半球上每个立体角的可见性的和，然后在取平均

黄色框：间接光照的radiance是个常数，BRDF是漫反射，cos积分是PI，所以结果就是 radiance * 漫反射系数，这两个值我们都不知道是多少，所以就随便指定一个得了。

深入理解1 ：

![image-20231122113113155](F:\learn\games202homework\image\image-20231122113113155.png)

这个步骤就是f(x)的在g(x)范围上的平均

深入理解2 ：

![image-20231122113236930](F:\learn\games202homework\image\image-20231122113236930.png)

![image-20231122113536917](F:\learn\games202homework\image\image-20231122113536917.png)

这个拆分就是把cosdw看作是一个整体，这个整体就是对单位圆上的一个微小的面积。

## 2 简单理解

![image-20231122113839153](F:\learn\games202homework\image\image-20231122113839153.png)

1 间接光照是常数

2 BRDF是常数

3 渲染方程直接就变成了常数 * 对visibility半球上的积分

所以我们需要计算的就是在屏幕空间，对visibility积分的平均

## 3 求解可见项平均

![image-20231122114808103](F:\learn\games202homework\image\image-20231122114808103.png)

1 我们采样，对每个shadering point为中心，半径为R的球上采样

2 如果采样点的深度小于深度图的深度，那么visibility就是1可见，

如果大于深度图的深度，visibility就是0不可见，这样近似来做。

但是这样做会有问题，就像图2展示的问题。还有问题就是我们对球上采样，但实际上我们应该对shadering point的法线方向上的半球采样，因为背面的光线肯定不会对我们的shadering point有贡献。

![image-20231122115658792](F:\learn\games202homework\image\image-20231122115658792.png)







# 6 Screen Space Reflect(SSR)

基础思路：

![image-20231121221032590](F:\learn\games202homework\image\image-20231121221032590.png)

1 我们第一次着色会获取每个像素的深度值，然后保存到贴图中，就像深度贴图那样

2 然后再一次着色会计算屏幕空间中每个像素点的法线，世界坐标位置，漫反射率，可见性信息，直接光照的颜色然后保存到贴图中

3 然后再一次着色会通过SSR计算出间接光照，然后通过直接光照和间接光照相加，得到最终结果

## 1 我们如何能知道采样点是由哪个间接采样点照亮的呢？

也就是采样点的反射光线会在场景中打到哪个采样点？

![image-20231121221914079](F:\learn\games202homework\image\image-20231121221914079.png)

可以采样逼近的方式：

黄色的点就是我们的采样点，虚线就是我们的反射光线，绿色的点就是间接采样点。

1 设置一个光线沿着wo移动的步径，每次通过这个步径获得位置点（橙色）

2 将位置点的深度值（mvp变换之后，透视除法之前，w分量就是深度值）和深度贴图中的值（将位置点变换到屏幕空间得到uv，通过uv采样深度贴图）比较，如果大于那这个点就是间接采样点



## 2 那么我们步径该取多大呢？

可以通过mipmap的方式改进

![image-20231121223048550](F:\learn\games202homework\image\image-20231121223048550.png)

正常我们说mipmap的存储就是生成一个图像金字塔，最精细的图在最下边，然后边长都变成一半一次递减，1层中的像素点是0层中的四个像素点的平均，一次类推。

而我们这种mipmap唯一的区别就是1层中的像素点是0层中的四个像素点的最小值。

![image-20231121223556544](F:\learn\games202homework\image\image-20231121223556544.png)

![image-20231121224150604](F:\learn\games202homework\image\image-20231121224150604.png)

逻辑就是，在和深度图的深度比较的时候，如果位置点的深度值小于level0层的深度，那么下一次就和level1层的深度比较，直到大于levelx层的深度，找到x层之后在减层数，这样循环往复的方式找到第一次大于深度图的像素位置。

## 3 看起来很好，但是有什么问题呢？

![image-20231121224534749](F:\learn\games202homework\image\image-20231121224534749.png)

1 屏幕空间的问题，深度图保存的只是一张二位图片，他不知道场景中的这是一个什么物体，也就是没有体积，那么反射出来的效果就是一张空壳，没有体积的概念。

![image-20231121224604873](F:\learn\games202homework\image\image-20231121224604873.png)

![image-20231121224714872](F:\learn\games202homework\image\image-20231121224714872.png)

2 他的范围有限制，因为深度图保存的只是一张摄像机所能看到的一张图片，所以反射出来的很短，只能反射出摄像机看到的东西，可以根据反射光线走的距离来进行一个模糊。

## 4 shadering中该如何做呢？

![image-20231121224901908](F:\learn\games202homework\image\image-20231121224901908.png)

我们SSR只是做了光线追踪的部分，其余所有的东西该怎么做就怎么做，也就是解渲染方程。

1 如果是glossy的采样点，反射就是一条光线，那么就没有积分

2 如果是diffuse的采样点，反射是一个lobe，那么就可以通过蒙特卡洛积分的方式求解

3 这里需要注意的是间接光照点的BRDF我们认为是diffuse的，因为我们看到间接光照点的颜色是该点反射到摄像机上的radiance，而我们不知道该点反射到采样点的radiance是多少，所以我们假定BRDF是diffuse的，那么简介光照点反射出的radiance都是相等的了

# 8 Real-Time Ray-Tracing(RTRT)

![image-20231126153913840](F:\learn\games202homework\image\image-20231126153913840.png)

1 spp：每个像素采样一次

![image-20231126153950268](F:\learn\games202homework\image\image-20231126153950268.png)

1 spp 需要的光线就有三条

![image-20231126154030744](F:\learn\games202homework\image\image-20231126154030744.png)

光线追踪里面最关的技术就是降噪！！！

## 1 Temporal 时间上的降噪

![image-20231126155713447](F:\learn\games202homework\image\image-20231126155713447.png)

降噪的非常重要的想法就是时间上的一个滤波，思路 就是：

1 对当前帧的画面降噪，也就是空间上的降噪。得到图p1

2 在当前帧的画面上的采样点，投影回local space，然后投影回上一帧的screen space得到颜色，组成了一张图p2

3 将图p1和图p2通过某种插值到一张图上，当作当前帧的画面，然后这样循环递归

### 第一步 空间上的降噪



![image-20231129222414025](F:\learn\games202homework\image\image-20231129222414025.png)

空间上的滤波我们通过高斯滤波核来进行，大致的算法流程就是通过高斯滤波核算出每个周边采样点 i 对中心采样点 j 的权重，将所有周边采样点的权重相加，将所有周边采样点的颜色相加，然后相除，得到滤波后的值，很简单吧。

我们使用联合双边滤波的方式才进行过滤：在作业中我们的滤波核选取：

![image-20231129222908432](F:\learn\games202homework\image\image-20231129222908432.png)

1 第一项就是两个点的距离，距离越大的化，周边采样点对中心采样点的贡献应该越小

2 第二项就是颜色的一样程度，如果颜色差距过大，那周边采样点对中心采样点的贡献也应该越小

3 第三项就是立方体任意相邻的两个面都不应该有贡献，所以这里就是两个采样点法线的夹角越大，那周边采样点对中心采样点的贡献也应该越小

![image-20231129223703173](F:\learn\games202homework\image\image-20231129223703173.png)

4 第四项就是：

![image-20231129223650513](F:\learn\games202homework\image\image-20231129223650513.png)

5 然后算出这个权重后，别忘了算颜色的平均，这样就得到了空间上过滤的结果图p1

### 第二步 上一帧结果

![image-20231129224001271](F:\learn\games202homework\image\image-20231129224001271.png)

通过这个式子，i 表示帧的序号，P投影矩阵 V是摄像机矩阵 M是模型矩阵

将当前帧世界坐标变换到前一帧的screen space下的坐标，进而获取到上一帧的颜色，这样就得到了上一帧的结果图p2

### 第三步 插值图p1和图p2

![image-20231129224256904](F:\learn\games202homework\image\image-20231129224256904.png)

如果采样点这一帧和前一帧的颜色差距过大的化，我们就需要将前一帧的颜色拉回一个正常范围里面

经过这三步就得到了当前帧的图片
