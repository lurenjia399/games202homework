#  1 Image base lighting(LBL)

shading from Environment Lighting

## 1 Split Sum(分布求和)

![](F:\learn\games202homework\hw2\image\image-20231113222940203.png)

如何得到每个采样点的颜色呢？就需要解渲染方程

1 光源项：方程中的光源项就是环境贴图中记录的rgb颜色，但是会有问题（1 在shader中进行samples就会很慢，每个采样点都需要进行计算。2 解渲染方程可以通过蒙特卡洛积分的方式，那也需要大量的数据才行）基于以上的问题所以我们希望不进行采样就能获取到光源项。

![image-20231113223640073](F:\learn\games202homework\hw2\image\image-20231113223640073.png)

2 BRDF项：通过观察，当BRDF光滑的时候，反射的W0范围比较小

当FBRDF粗糙的时候，反射就在半球上，比较均匀，也就是平滑。

根据以上两个观察就能将光源项提出来，并单位化

![image-20231113224326923](F:\learn\games202homework\hw2\image\image-20231113224326923.png)

### 1 那么黄色框这部分代表什么呢？光源的颜色 / 一个范围

![image-20231113225309226](F:\learn\games202homework\hw2\image\image-20231113225309226.png)

代表的就是一个滤波，也就是获取到当前像素点的颜色，然后除以一小块范围

，在写回这个像素点。通俗点就是一个模糊操作。

（注意这可以预计算也就是提前算好，然后再渲染的过程中使用。预计算根据滤波核的大小生成不同的图片，在shader中根据BRDF的W0的大小确定滤波核，然后就能通过双线性插值类似mipmap的方式采样图中的颜色）。

![image-20231113225546658](F:\learn\games202homework\hw2\image\image-20231113225546658.png)

根据这幅图，我们就可以通过r的方向来查询预计算出的结果来得到出射光的radiance。

### 2 下面看剩下的部分，一个BRDF积分

![image-20231113230306065](F:\learn\games202homework\hw2\image\image-20231113230306065.png)

我们也可以采用预计算的方式，也就是通过遍历所有的参数来获得BRDF项

在微表面模型下，我们的BRDF为

![image-20231113231141766](F:\learn\games202homework\hw2\image\image-20231113231141766.png)

其中：F() D() 分别为

![image-20231113231157824](F:\learn\games202homework\hw2\image\image-20231113231157824.png)

G()为

![image-20231113231637007](F:\learn\games202homework\hw2\image\image-20231113231637007.png)

也就是三个参数组成，但是我们觉得参数还是太多了，还想要降维

![image-20231113232213927](F:\learn\games202homework\hw2\image\image-20231113232213927.png)

通过菲涅尔的公式F项，这边进行一个纯数学的展开，将R0基础反射率提出来，这里手动在纸上算下就行。

至此，我们就将BRDF项中参数降到两维了，一个式roughness,一个是角度。也就是可以与计算出一张图了。

![image-20231113232650802](F:\learn\games202homework\hw2\image\image-20231113232650802.png)

### 3 结果：

![image-20231113232853490](F:\learn\games202homework\hw2\image\image-20231113232853490.png)

![image-20231113232943590](F:\learn\games202homework\hw2\image\image-20231113232943590.png)

# 2 Precompute Radiance Transfer(PRT)

## 1 前导知识

### 1 卷积

![image-20231115103439390](F:\learn\games202homework\hw2\image\image-20231115103439390.png)

时域上模糊的操作就是频域上的乘积，也就是卷积。

### 2 基函数

![image-20231115104032143](F:\learn\games202homework\hw2\image\image-20231115104032143.png)

基函数的表示

### 3 球谐函数

![image-20231115104525709](F:\learn\games202homework\hw2\image\image-20231115104525709.png)

任何一个二维函数都能用球谐函数表示，类似于一维里面的傅里叶展开

![image-20231115105601835](F:\learn\games202homework\hw2\image\image-20231115105601835.png)

任何一个二维函数 * 任意一个球谐函数的基函数 在积分 就能得到对应基函数的系数，这个过程也叫投影。

类比于一个向量，他就能用三个基向量的系数来表示。

两个函数相乘求积分被称为函数内积，这个操作就相当于求两个向量内积，也就是点乘。

## 2 PRT

![image-20231115113929225](F:\learn\games202homework\hw2\image\image-20231115113929225.png)

渲染方程中的三个积分项都可以看作是球面函数 

![image-20231115114720356](F:\learn\games202homework\hw2\image\image-20231115114720356.png)

作出假设，场景中的所有东西都不变，将渲染方程就看作两部分。

lighting：将其用基函数的方式表示。

Lighting transport：一个shadering point 上的性质，可以看作是这样，因为场景中所有东西都不变。

然后我们分成情况分析：

### 1 diffuse 

![image-20231115233234921](F:\learn\games202homework\hw2\image\image-20231115233234921.png)

1 diffuse的情况下BRDF是一个常数可以提出来

2 lighting项可以通过预计算得到，从每个环境贴图的像素中读取光照颜色，然后将其投影到基函数上得到系数，那么原光源就等于图中黄色的部分，每个系数乘上基函数的和 等于光源

3 然后通过数学变换，就可以看出剩余部分也可以通过预计算，将可见项投影到基函数上得到系数

4 最终的结果就是 两个系数的乘积在求和，别忘了乘上BRDF这个常数

这里再写作业的时候，需要注意的就是我们的这些系数都是RGB三个通道来表示，所以就是在各个通道上我们要求一个系数的乘积和，最后得到Shadering的值

### 2 glossy

glossy 的物体和diffuse的物体有什么区别呢？他们只是在BRDF上有区别，

diffuse物体的BRDF是一个常数也就是无论我们从哪个方向看采样点的颜色都是一样的，也就是个二维的函数，只和入射角有关

glossy物体的BRDF就不是一个常数了，我们从不同的方向看过去得到的结果都是不一样的，也就是个四维的函数，和入射角和出射角都有关系

![image-20231116000529604](F:\learn\games202homework\hw2\image\image-20231116000529604.png)

第一步：我们通过PRT近似算出了shadering的结果，但是Lt项是一个关于出射角的函数。（可以这样看，我们固定一个出射角，那么LightTransport项都只和入射角有关，也就是可以看成球面函数了，然后通过投影基函数就能得出Lt，所以也就是Lt项是一个关于出射角的函数，每给定一个出射角Lt函数都能得出具体的Lt值）

![image-20231116001101922](F:\learn\games202homework\hw2\image\image-20231116001101922.png)

第二步：我们得到了关于o的Lt函数，那么我们就可以对o投影到基函数上，那么就得到了最终的式子。

![image-20231116001304362](F:\learn\games202homework\hw2\image\image-20231116001304362.png)

所以也就能看出L项是一个向量，每个小格子代表一个系数，Lt项是一个矩阵，每个小格子也是一个系数，他俩的乘积得到3阶sh函数的9个系数，然后在分别乘上o向上的基函数求和，就能算出最终的shadering。