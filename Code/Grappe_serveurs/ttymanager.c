#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<termios.h>
#include<sys/select.h>
#include<sys/ioctl.h>
#include<fcntl.h>
#include<errno.h>
#include<dirent.h>

#include<string>
#include<map>
#include<fstream>
#include<sstream>
#include<cctype>

std::map<std::string, int> g_list;

bool ttych(int p_fd, char* p_command)
{
    return ioctl(p_fd, TIOCSTI, p_command) != -1;
}

int iskb(void)
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

int getch(void)
{
    unsigned char c;
    (void)read(0, &c, sizeof(c));
    return c;
}

std::string bash_pid(int p_pid)
{
    struct dirent *lecture;
    char l_buffer[4096];
    DIR *rep;

    rep = opendir("/proc/" );
    while ((lecture = readdir(rep)))
    {
        std::string l_pid = lecture->d_name;
        bool has_only_digits = (l_pid.find_first_not_of("0123456789") == std::string::npos);
        if (has_only_digits)
        {
            std::string l_file = "/proc/"+l_pid+"/stat";
            std::ostringstream l_string;
            l_string << " (ssh) S " << p_pid << " ";

            FILE* l_fichier = fopen(l_file.c_str(), "r");
            if (l_fichier)
            {
                while(!feof(l_fichier))
                {
                    (void)fgets(l_buffer, 4096, l_fichier);
                    if (std::string(l_buffer).find(l_string.str()) != std::string::npos)
                    {
                        printf("FOUND: %s\n", lecture->d_name);
                        fclose(l_fichier);
                        closedir(rep);

                        int l_value;
                        sscanf(l_buffer, "%d ", &l_value);

                        l_string.str("");
                        l_string << "/proc/" << l_value << "/fd/0";

                        printf("return %s\n", l_string.str().c_str());
                        return  l_string.str();
                    }
                }
               fclose(l_fichier);
            }
        }
    }
    closedir(rep);

    return "";
}

int run_xterm(std::string p_command)
{
    int pid = fork();
    if(pid < 0)
    {
        printf("Error");
        exit(1);
    }
    else if (pid == 0)
    {
        execl("/usr/bin/xterm", "/usr/bin/xterm", "-geometry", "60x15", "-e", p_command.c_str(),  NULL);
    }
    else
    {
        std::string l_value = "";
        while(l_value == "")
        {
            l_value = bash_pid(pid);
            if (l_value != "")
            {
                printf("(%s)\n", l_value.c_str());
                g_list[l_value] = open(l_value.c_str(), O_RDWR);
                break;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    std::map<std::string, int>::iterator l_iter;

    for (int i=1; i<argc; i++)
    {
        run_xterm(argv[i]);
    }

    while(1)
    {
         if (iskb())
         {
             char buffer[2];
             sprintf(buffer, "%c", getch());

             int l_count = 0;
             for (l_iter=g_list.begin(); l_iter!=g_list.end(); l_iter++)
             {
                 if (l_iter->second!=-1 && ttych(l_iter->second, buffer) == false)
                 {
                     l_iter->second = -1;
                 }
                 else
                 {
                     l_count++;
                 }
             }
             if (l_count == 0) break;
         }
    }
}

