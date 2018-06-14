#include <stdio.h>
#include <signal.h>
#include <errno.h>

#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

volatile sig_atomic_t quit;

static void
quit_handler(int sig)
{
    quit = 1;
}

int
main(int argc, char **argv)
{
    int fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

    if (fd == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_nl snl = {
        .nl_family = AF_NETLINK,
        .nl_groups = RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR,
    };

    if (bind(fd, (struct sockaddr *)&snl, sizeof(snl)) == -1) {
        perror("bind");
        return 1;
    }

    static unsigned char buffer[4094];

    while (!quit) {
        ssize_t ret = recv(fd, buffer, sizeof(buffer), 0);

        if (ret == 0)
            break;

        if (ret == -1) {
            switch (errno) {
            case EAGAIN:
            case EINTR:
                perror("recv");
            default:
                break;
            }
            continue;
        }

        if ((size_t)ret > sizeof(buffer))
            continue;

        struct nlmsghdr *nlh = (struct nlmsghdr *)buffer;

        if (nlh->nlmsg_flags & MSG_TRUNC)
            continue;

        int len = ret;

        while ((NLMSG_OK(nlh, len)) && (nlh->nlmsg_type != NLMSG_DONE)) {
            if ((nlh->nlmsg_type == RTM_NEWADDR) ||
                (nlh->nlmsg_type == RTM_DELADDR)) {

                struct ifaddrmsg *ifa = (struct ifaddrmsg *)NLMSG_DATA(nlh);
                struct rtattr *rth = IFA_RTA(ifa);
                int rtl = IFA_PAYLOAD(nlh);

                while (rtl && RTA_OK(rth, rtl)) {
                    if (rth->rta_type == IFA_LOCAL) {
                        char tmp[1024];
                        if (inet_ntop(ifa->ifa_family, RTA_DATA(rth), tmp, sizeof(tmp)))
                            printf("%s %s\n", (nlh->nlmsg_type == RTM_NEWADDR) ? "ADD" : "DEL", tmp);
                    }
                    rth = RTA_NEXT(rth, rtl);
                }
            }
            nlh = NLMSG_NEXT(nlh, len);
        }
    }

    return 0;
}
