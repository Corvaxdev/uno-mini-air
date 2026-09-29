# Direct HTTP load evidence

Tested 28 September 2026: **0.11.15-hidden**, before Room Terra. Two external
VPS used the public DNS name and normal Internet path. Half the clients read
sensors five seconds after replies; half watched the old Life world at roughly
one-second cadence. Starts were spread over five seconds. These were synthetic
HTTP clients, not full browsers: no DOM, browser-sized headers or analytics.
Each response body was validated. Data/page deadlines: 3.5/5 seconds.

| Simulated visitors | Duration | Valid / attempted | Success |
|---|---:|---:|---:|
|2 mixed|90 s|108/108|100.00%|
|10 mixed|60 s|317/321|98.75%|
|20 mixed|60 s|416/459|90.63%|
|40 mixed|60 s|546/708|77.12%|
|10 mixed repeat|120 s|475/527|90.13%|
|2 mixed after reset|600 s|729/734|99.32%|

The 10-client repeat lost LAN/public connectivity near 97 seconds. All 14
recovery probes failed; a manual UNO reset restored service. Cause is unresolved.
The ten-minute run was after that reset. Latencies from successful requests
alone cannot hide these failures. No stable current client capacity is inferred.

Historical 0.12.3 Terra diagnostic: 90 pairs of sensor/world reads from one
external VPS over 90 seconds, 180/180 valid replies; pair median/p95 251/779 ms.
This is a functional check, not a multi-client benchmark. Current 0.12.9 changes
light modelling and UI; those older tests do not validate its peak capacity.

Sanitized aggregate: [direct-http-20260928.json](direct-http-20260928.json).
The original private run records retain failures and deployment metadata.
Public aggregates deliberately omit host identities and local network details.
