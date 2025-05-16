NAME = ircserv
TEST_NAME = test_client
CC = c++
CFLAGS = -Wall -Wextra -Werror -std=c++98

SRCS = src/Utils.cpp \
       src/Server.cpp \
       src/Client.cpp \
       src/Channel.cpp \
       src/CommandHandler.cpp \
       src/main.cpp

OBJS = $(SRCS:.cpp=.o)

INCLUDES = -I include

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(NAME)

test: $(TEST_NAME)

$(TEST_NAME): test_client.cpp
	$(CC) $(CFLAGS) $(INCLUDES) test_client.cpp -o $(TEST_NAME)

%.o: %.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME) $(TEST_NAME)

re: fclean all

.PHONY: all clean fclean re test 
