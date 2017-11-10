# game_server


采用lua5.3.4,底层全部用C来实现，参考了一些skynet的东西，但是还是多进程单线程的架构。


网络库参考redis网络框架，并精简了一些东西，只用了epoll，另外，timer部分改成了时间轮。

代码风格也参考了redis，另外，lua与c交互的部分，参考了skynet。