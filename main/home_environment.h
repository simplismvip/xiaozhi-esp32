#ifndef HOME_ENVIRONMENT_H
#define HOME_ENVIRONMENT_H

#include <string>

struct HomeEnvironmentData {
    std::string weather;
    std::string temp;
    std::string humidity_text;
};

// GET CONFIG_HOME_ENV_URL and parse weather/temp/humidity_text.
// Returns false if URL empty, network error, or JSON invalid.
bool FetchHomeEnvironment(HomeEnvironmentData& out);

#endif  // HOME_ENVIRONMENT_H
