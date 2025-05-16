#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>
#include <sstream>

class Utils {
public:
    // String işlemleri
    static std::vector<std::string> split(const std::string& str, char delimiter);
    static std::string trim(const std::string& str);
    static bool isNumeric(const std::string& str);
    
    // Hata yönetimi
    static void error(const std::string& message);
    static void log(const std::string& message);
    
    // Socket işlemleri için yardımcı fonksiyonlar
    static int createSocket();
    static void setNonBlocking(int fd);
    static void bindSocket(int sockfd, int port);
    static void listenSocket(int sockfd);
    
    // IRC protokolü için yardımcı fonksiyonlar
    static std::string formatMessage(const std::string& prefix, const std::string& command, const std::string& params);
    static bool isValidNickname(const std::string& nickname);
    static bool isValidChannelName(const std::string& channel);
    
    static std::string getServerCreationTime();
    
private:
    Utils(); // Singleton pattern için private constructor
    ~Utils();
    Utils(const Utils& other); // Copy constructor
    Utils& operator=(const Utils& other); // Assignment operator
};

#endif 
