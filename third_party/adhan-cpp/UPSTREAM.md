# Adhan C++

Source: https://github.com/Lotconn/adhan-cpp/releases/tag/v1.0.2

Release: **v1.0.2**, commit `8d65a2906dbdb90a84d30d385a1b5f84e38d0c1f`.
The `include/` and `src/` trees and LICENSE are copied unchanged from that commit.
License: MIT, copyright Ishraq Hasan. The project is a port of
[Batoul Apps Adhan](https://github.com/batoulapps/adhan-js), also MIT.
Its original copyright notice is preserved in `LICENSE.adhan-js`.

OpenAthan compiles it as C++20 with ESP-IDF C++ exceptions enabled rather than
changing upstream error handling. OpenAthan validates dates, finite coordinates,
and preset selection before calling it. Unavailable polar events remain absent.
No timezone database or network is needed by the library. The caller supplies
the local civil date; results are UTC instants. OpenAthan implements scheduling
separately in its framework-independent core.

Updates must use a stable release, retain attribution, compare both source trees,
rerun the reference tests, and repeat the full firmware size measurements.
