# tinyhttpd
写了大量注释的一个版本，以方便学习 tinyhttpd 的源码

更多介绍请看 https://www.ccgxk.com/cpp/77.html

## 编译环境

本地我使用的是：macbook air 2020 M1 + gcc version 13.0.0

## 两种编译方法

### 第一种

1. 终端到 httpd 目录
2. 终端输入 make          # 这是编译httpd
3. cd htdocs
4. g++ color.cpp -o color.cgi          # 这是编译 color.cpp 成 cgi 文件
5. cd ..
6. ./httpd


打开浏览器，http://127.0.0.1:4000 即可看到结果

不出意外，在表单中输入 red ,网页背景颜色会变红

### 第二种

1. gcc httpd.c
2. cd htdocs
3. g++ color.cpp -o color.cgi
4. cd ..
5. ./a.out

打开浏览器，http://127.0.0.1:4000 即可看到结果

不出意外，在表单中输入 red ,网页背景颜色会变红
