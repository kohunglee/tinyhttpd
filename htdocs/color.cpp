#include <iostream>
#include <unistd.h>
#include <stdlib.h>

int main()
{
    char *data;
    char *length;
    char color[20];
    char c = 0;
    int flag = -1;

    std::cout << "Content-Type: text/html\r\n" << std ::endl;  // 头部信息
    std::cout << "<HTML><TITLE>color world</TITLE>" 
                 "<h1>hello</h1>"
                 "<BODY><P>the color is:"<< std ::endl;
    if((data = getenv("QUERY_STRING")) != NULL)
    {
        while(*data != '=')
            data++;
        data++;
        sprintf(color,"%s",data);
    }
    if((length = getenv("CONTENT_LENGTH")) != NULL)
    {
        int i;
        for(i = 0; i < atoi(length); i++)
        {
            read(STDIN_FILENO,&c,1);
            if(c == '=')
            {
                flag = 0;
                continue;
            }
            if(flag > -1)
            {
                color[flag++] = c;
            }
        }
        color[flag] = '\0';
    }
    std::cout << color << std::endl;
    std::cout << "<body bgcolor = \"" << color << "\"/>" << std::endl;

    std::cout << "<BODY></HTML>" << std::endl;
    return 0;

}
