#pragma once
namespace esphome {struct Application{unsigned reboots{};void safe_reboot(){++reboots;}};inline Application App;}
