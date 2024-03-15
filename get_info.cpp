#include <SFML/Graphics.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <libssh/libssh.h>

nlohmann::json& init_setting(){
    std::ifstream file("setting.json");
    static nlohmann::json setting;
    file >> setting;
    file.close();
    return setting;
}

std::string execute_command(ssh_session session, const std::string& command) {
    ssh_channel channel;
    int rc;
    char buffer[256];
    unsigned int nbytes;
    std::string output;

    channel = ssh_channel_new(session);
    if (channel == NULL) return "Error creating SSH channel";

    rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK) {
        ssh_channel_free(channel);
        return "Error opening SSH channel";
    }

    rc = ssh_channel_request_exec(channel, command.c_str());
    if (rc != SSH_OK) {
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return "Error executing command";
    }

    nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
    while (nbytes > 0) {
        output.append(buffer, nbytes);
        nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
    }

    if (nbytes < 0) {
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return "Error reading from SSH channel";
    }

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);

    return output;
}

int main()
{
    nlohmann::json setting = init_setting();
    sf::RenderWindow window(sf::VideoMode::getFullscreenModes()[0], "SERVER_INFO", sf::Style::Default);
    sf::Font font;
    if (!font.loadFromFile("font/"+setting["font"].get<std::string>())) {
        // Handle font loading error
        return 1;
    }

    // Initialize SSH session
    ssh_session ssh_session = ssh_new();
    if (ssh_session == NULL) return -1;

    // Set SSH options
    ssh_options_set(ssh_session, SSH_OPTIONS_HOST, setting["host"].get<std::string>().c_str());
    ssh_options_set(ssh_session, SSH_OPTIONS_USER, setting["user"].get<std::string>().c_str());
    ssh_options_set(ssh_session, SSH_OPTIONS_PORT, &setting["port"]);

    // Connect to server
    int rc = ssh_connect(ssh_session);
    if (rc != SSH_OK) {
        ssh_free(ssh_session);
        return -1;
    }

    // Authenticate
    rc = ssh_userauth_password(ssh_session, NULL, setting["password"].get<std::string>().c_str());
    if (rc != SSH_AUTH_SUCCESS) {
        ssh_disconnect(ssh_session);
        ssh_free(ssh_session);
        return -1;
    }

    // Execute tasks and display results
    std::string result = execute_command(ssh_session, "top");
    sf::Text message(result, font, 24);
    message.setFillColor(sf::Color::White);
    message.setPosition(window.getSize().x / 2 - message.getGlobalBounds().width / 2,
                        window.getSize().y / 2 - message.getGlobalBounds().height / 2);
    
    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        window.clear();
        window.draw(message);
        window.display();
    }

    // for (const auto& task : setting["tasks"]) {
    //     std::string result = execute_command(ssh_session, task["command"].get<std::string>());
    //     sf::Text message(result, font, 24);
    //     message.setFillColor(sf::Color::White);
    //     message.setPosition(window.getSize().x / 2 - message.getGlobalBounds().width / 2,
    //                         window.getSize().y / 2 - message.getGlobalBounds().height / 2);

    //     while (window.isOpen())
    //     {
    //         sf::Event event;
    //         while (window.pollEvent(event))
    //         {
    //             if (event.type == sf::Event::Closed)
    //                 window.close();
    //         }

    //         window.clear();
    //         window.draw(message);
    //         window.display();
    //     }
    // }

    ssh_disconnect(ssh_session);
    ssh_free(ssh_session);

    return 0;
}