# Pukpukus
ESP8266 based toilet traffic solution.

# the Problem
There is only 1 cabin per 20 men in our new office. It was annoying to enter toilet and realize that both still occupied. On 6th try... someone still shiting there... 1 hour passed (O_O)

# the Solution
Traffic light based on readings from door sensors.

1. Device is WiFi AP and HTTP server with API. With 8x8 bi-color matrix (as traffic light) and OLED display for stats.
2. Device is WiFi client and HTTP client with connected 2 reed switches and RGB led for current status.
