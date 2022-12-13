# 例子 / Examples

这个例子用linux socketcan. Windows用户可以用个linux虚拟机。WSL应该也可以。

This example uses linux socketcan. Windows users can run it in a linux VM. It might work under WSL.


# socketcan 虚拟接口设置 / virtual interface setup
```sh
# 设置虚拟socketCAN接口
# setup a virtual socketCAN interface
sudo ip link add name vcan0 type vcan
sudo ip link set vcan0 up
```
# 构建例子 / building the example
```
make example
```

# 在vcan0接口上运行例子服务器 / run the server on vcan0
```sh
./server vcan0
# 服务器会一直跑。用 ctrl+c 推出
# The server will run continuously. Use ctrl+c to exit
```

# 在vcan0接口上运行客户端 (打开另一个终端) / run the client on vcan0 (open another shell)
```sh
./client vcan0
# 客户端跑完流程后会自动退出
# The client will exit after it has executed its sequence
```

# 用candump看看输出
```sh
> candump -tz vcan0
 (000.000000)  vcan0  111   [4]  01 02 03 04
 (000.000028)  vcan0  111   [4]  01 02 03 04
 (000.000090)  vcan0  701   [8]  02 11 01 00 00 00 00 00  # 0x11 ECU复位请求
 (000.010199)  vcan0  700   [8]  02 51 01 00 00 00 00 00  # 0x11 ECU复位肯定响应
 (000.010213)  vcan0  701   [8]  05 22 00 01 00 08 00 00  # 0x22 RDBI请求
 (000.020318)  vcan0  700   [8]  10 1B 62 00 01 00 00 08  # ISO-TP流控框
 (000.020326)  vcan0  701   [8]  30 08 00 00 00 00 00 00  # 0x22 RDBI请求
 (000.030416)  vcan0  700   [8]  21 49 27 6D 20 61 20 55  # 0x22 RDBI响应 (1)
 (000.040674)  vcan0  700   [8]  22 44 53 20 73 65 72 76  # 0x22 RDBI响应 (2)
 (000.050829)  vcan0  700   [8]  23 65 72 20 20 20 20 00  # 0x22 RDBI响应 (3)
 (000.051509)  vcan0  701   [8]  02 10 03 00 00 00 00 00  # 0x10 会话控制
 (000.072713)  vcan0  700   [8]  03 7F 10 33 00 00 00 00  # 0x10 会话控制否定响应
 (000.072979)  vcan0  701   [8]  02 11 04 00 00 00 00 00  # 0x11 ECU复位请求
 (000.124015)  vcan0  700   [8]  03 51 04 FF 00 00 00 00  # 0x11 ECU复位肯定响应

```


# 也可以用python-udsoncan实现客户端 / You can also use python-udsoncan to implement a client

```sh
# 在另外一个终端，安装python依赖性
# In another shell, install the required python packages
pip3 install -r example/requirements.txt

# 然后运行客户端
# then run the client
./example/client.py vcan0
```
