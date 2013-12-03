#!/bin/sh
#
# tinydns: an init script to start & stop the tinydns service daemon.
#
# chkconfig: - 20 80
# description: tinydns is a DNS server program.
#              tinydns server maps & serves IP addresses for a given domain
#              name from a local static database of domain names.
#

### BEGIN INIT INFO
# Provides:          tinydns
# Required-Start:    $network
# Required-Stop:     $network
# Default-Stop:      0 1 2 3 4 5 6
# Short-Description: start and stop tinydns daemon at boot time.
# Description:       tinydns is a DNS server program.
### END INIT INFO

# Source function library.
. /etc/init.d/functions

# Source networking configuration
. /etc/sysconfig/network

prog=PREFIX/sbin/tinydns
config=/etc/ndjbdns/tinydns.conf
logfile=/var/log/tinydns.log
lockfile=/var/lock/subsys/tinydns
pidfile=/var/run/tinydns.pid
port=53

[ -e /etc/sysconfig/$prog ] && . /etc/sysconfig/$prog

start() {
    [ -x $prog ] || exit 5
    [ -f $config ] || exit 6
    echo -n $"Starting ${prog##[a-z/.]*/}: "
    daemon --pidfile="$pidfile" $prog -c $config -l $logfile -p $pidfile -w $port -D 2>> $logfile
    RETVAL=$?
    echo
    [ $RETVAL -eq 0 ] && touch $lockfile
    return $RETVAL
}

stop() {
    echo -n $"Stopping ${prog##[a-z/.]*/}: "
    killproc -p "$pidfile" $prog
    RETVAL=$?
    echo
    [ $RETVAL -eq 0 ] && rm -f $lockfile
    return $RETVAL
}

restart() {
    stop
    start
}

reload() {
    restart
}

force_reload() {
    restart
}

rh_status() {
    status -p "$pidfile" $prog
}

rh_status_q() {
    rh_status >/dev/null 2>&1
}


case "$1" in
    start)
        rh_status_q && exit 0
        $1
        ;;
    stop)
        rh_status_q || exit 0
        $1
        ;;
    restart)
        $1
        ;;
    reload)
        rh_status_q || exit 7
        $1
        ;;
    force-reload)
        force_reload
        ;;
    status)
        rh_status
        ;;
    condrestart|try-restart)
        rh_status_q || exit 0
        restart
        ;;
    *)
        echo $"Usage: $0 {start|stop|status|restart|condrestart|try-restart|reload|force-reload}"
        exit 2
esac
exit $?
