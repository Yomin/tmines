.PHONY: all, clean

NAME=tmines

all: $(NAME)

$(NAME): $(NAME).c
	gcc -Wall -o $(NAME) $(NAME).c -lncurses

clean:
	rm -rf $(NAME)
