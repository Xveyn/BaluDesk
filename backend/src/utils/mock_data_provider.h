#pragma once

#include "system_info.h"
#include "raid_info.h"
#include <vector>
#include <string>
#include <cstdint>

namespace baludesk {

/**
 * Power monitoring data structure
 */
struct PowerMonitoring {
    double currentPower;        // Current power consumption in Watts
    double energyToday;         // Total energy consumed today in kWh
    double trendDelta;          // Power trend delta in Watts (+/- from average)
    int deviceCount;            // Number of monitored devices
    double maxPower;            // Maximum power capacity in Watts (for progress calculation)
};

/**
 * Network statistics data structure
 */
struct NetworkStats {
    double uploadSpeed;         // Current upload speed in bytes/s
    double downloadSpeed;       // Current download speed in bytes/s
    uint64_t totalUpToday;      // Total bytes uploaded today
    uint64_t totalDownToday;    // Total bytes downloaded today
};

/**
 * NAS service status
 */
struct NasServiceInfo {
    std::string name;
    std::string status;         // "running", "stopped", "unknown"
};

/**
 * Provides mock/test data for development and testing
 * Used when devMode is set to "mock" to return predictable test data
 */
class MockDataProvider {
public:
    /**
     * Get mock system information for dev-mode testing
     * Returns fixed test values: 45% CPU, 8 cores, 16GB RAM, 1TB disk
     * @return SystemInfo struct with mock data
     */
    static SystemInfo getMockSystemInfo();

    /**
     * Get mock RAID status for dev-mode testing
     * Returns 2 arrays: md0 (RAID1, optimal), md1 (RAID5, rebuilding @ 67.5%)
     * @return RaidStatus struct with mock data
     */
    static RaidStatus getMockRaidStatus();

    /**
     * Get mock power monitoring data for dev-mode testing
     * Returns: 87.3W current, 1.85 kWh today, -5.2W trend (decreasing), 3 devices
     * @return PowerMonitoring struct with mock data
     */
    static PowerMonitoring getMockPowerMonitoring();

    /**
     * Get mock network statistics for dev-mode testing
     * Returns: 2.4 MB/s up, 512 KB/s down, 1.2 GB total today
     * @return NetworkStats struct with mock data
     */
    static NetworkStats getMockNetworkStats();

    /**
     * Get mock NAS services status for dev-mode testing
     * Returns: Samba(running), SSH(running), Nginx(running), WebDAV(stopped)
     * @return vector of NasServiceInfo with mock data
     */
    static std::vector<NasServiceInfo> getMockServicesStatus();
};

} // namespace baludesk
