#!/bin/bash

#apt install -y cpufrequtils
echo 'GOVERNER="performance"' | tee /etc/default/cpufrequtils
systemctl disable ondemand
systemctl restart cpufrequtils

/opt/ckatsak/linux-nova/tools/power/cpupower/cpupower idle-set --disable-by-latency 0

# Edit /etc/systemd/system/disable_cpu_idle_states.serivce

#[Unit]
#Description=Disable idle CPU states
#After=cpufrequtils.service
#
#[Service]
#Type=oneshot
#ExecStart=/usr/bin/cpupower idle-set --disable-by-latency 0
#
#[Install]
#WantedBy=multi-user.target

# systemctl daemon-reload
# systemctl enable disable_cpu_idle_states
# reboot
