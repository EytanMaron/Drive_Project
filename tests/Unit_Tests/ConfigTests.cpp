#include <gtest/gtest.h>
#include "Services/EnvVarConfig.h"
#include <cstdlib>

#include <cstdlib>

/**
 * The EnvVarConfig implementation simply reads the STORAGE_DIR environment
 * variable. Tests run in many contexts (local build, docker build stage,
 * docker-compose, CI), so we explicitly seed the variable here to keep the
 * test deterministic and independent of the host configuration.
 */
TEST(ConfigTests, ReadExistingEnvVar) {
#ifdef _WIN32
    _putenv_s("STORAGE_DIR", "/tmp/data");
#else
    setenv("STORAGE_DIR", "/tmp/data", 1);
#endif
    EnvVarConfig config;
    EXPECT_EQ(config.getStoragePath(), "/tmp/data");
}

