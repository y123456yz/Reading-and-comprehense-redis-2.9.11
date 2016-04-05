# reading-redis-2.9.11
redis-2.9.11阅读理解，带详细注释

本份代码从https://github.com/huangz1990/redis-3.0-annotated clone下来，然后自己添加自己的理解，再次基础上增加函数调用流程注释。
参考数据<redis涉及实现>


阅读工具source insight,如果中文乱码，按照source insight configure目录中说明操作

本代码解决了huangz1990原始代码source insight中文乱码问题



阅读redis源码目的？
    了解内存数据库相关优秀数据结构，由于前期已经通读完毕nginx源码，阅读redis源码是为了配合nginx来实现静态资源加速，为后期提升nginx性能做准备。
	
阅读进度：本代码对redis源码主要功能进行了详细注释，并加上了自己的理解。redis源码大体阅读完毕，并注释添加了相关函数的调用流程。

注：由于redis集群功能官方暂时没有文档版本，因此redis集群源码暂时没有研究。等出了文档版本，在研究注释。






redis源码阅读完后，发现以下问题及改造点：


问题: 
	set 和 setbit没有做标记区分都采用REDIS_STRING类，编码方式也一样。
	如果set test abc,然后继续执行setbit test 0 1；是会成功的。会造成test键内容被无意修改。可以增加一种编码encoding方式来加以区分。


可以改造的地方:
	改造点1:
		在应答客户端请求数据的时候，全是epoll采用epool write事件触发，这样不太好，每次发送数据前通过epoll_ctl来触发epoll write事件，即使发送
	一个"+OK"字符串，也是这个流程。
		改造方法: 开始不把socket加入epoll，需要向socket写数据的时候，直接调用write或者send发送数据。如果返回EAGAIN，把socket加入epoll，在epoll的
	驱动下写数据，全部数据发送完毕后，再移出epoll。
		这种方式的优点是：数据不多的时候可以避免epoll的事件处理，提高效率。 这个机制和nginx发送机制一致。
		后期有空来完成该优化。

	改造点2:
		在对key-value对进行老化删除的过程中，采用从expire hash桶中随机取一个key-value节点，判断是否超时，超时则删除,这种方法每次采集点有很大
		不确定性，造成空转，最终由没有删除老化时间到的节点，浪费CPU资源。

		改造方法:在向expire hash桶中添加包含expire时间的key-value节点，在每个具体的table[i]桶对应的链中，可以按照时间从小到大排序，每次删除老化的时候
		直接取具体table[i]桶中的第一个节点接口判断出该桶中是否有老化时间到的节点，这样可以很准确的定位出那些具体table[i]桶中有老化节点，如果有则取出
		删除接口。

	改造点3:
		主服务器同步rdb文件给从服务器的时候是采用直接读取文件，然后通过网络发送出去，首先需要把文件内容从内核读取到应用层，在通过网络应用程序从应用层
		到内核网络协议栈，这样有点浪费CPU资源和内存。

		改造方法:通过sendfile或者aio方式发送，避免多次内核与应用层交互，提高性能。
	
	改造点4：
		主备同步每次都要写磁盘，然后从磁盘读，效率低下。
		
		改造方法：直接把内存中的key-value对由主传到备，不用落地。
	
	改造点5：
		如果主备之间网络不好，例如rdb文件落地成功，主读取rdb文件，然后通过网络向备传输，如果传输一般网络断开，当网络重新恢复后，备有重新走
			之前的流程，主又要写rdb文件到磁盘，然后重新读磁盘往备传送。如此反反复复，主会不停额写磁盘，读磁盘，传输，永远传输不完。
	
	权限控制能力不强，很容易被识破并攻击，就像wifi万能钥匙一样，很容易破密。
