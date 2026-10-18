#pragma once
namespace esphome {struct Application{unsigned reboots{};void feed_wdt() {} void safe_reboot(){++reboots;}};inline Application App;}
