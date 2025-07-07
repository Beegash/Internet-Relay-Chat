NAME = ircserv

SRC = src/main.cpp src/Server.cpp src/ServerNetwork.cpp src/ServerUtils.cpp src/ServerCommands.cpp src/Client.cpp src/Channel.cpp

OBJ = $(SRC:.cpp=.o)

CC = c++

CPPFLAGS = -Wall -Wextra -Werror -std=c++98

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CPPFLAGS) -Iinclude -o $(NAME) $(OBJ)

%.o: %.cpp
	$(CC) $(CPPFLAGS) -Iinclude -c $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
