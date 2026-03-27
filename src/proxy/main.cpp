#include "unillm/proxy.hpp"
#include "unillm/unillm.hpp"

#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
  try {
    const std::string config_path = argc > 1 ? argv[1] : "unillm.toml";
    unillm::ClientConfig config = unillm::UnifiedClient::load_config_file(config_path);

    unillm::ProxyServerOptions options;
    if (argc > 2) {
      options.port = static_cast<std::uint16_t>(std::stoi(argv[2]));
    }

    unillm::UnifiedClient client(std::move(config));
    unillm::ProxyServer server(std::move(client), options);
    std::cout << "unillm proxy listening on http://" << options.host << ':' << options.port << '\n';
    server.run();
  } catch (const std::exception& error) {
    std::cerr << "unillm proxy failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
