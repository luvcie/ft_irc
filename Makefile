NAME        = ircserv

CXX         = c++
CXXFLAGS    = -Wall -Wextra -Werror -std=c++98 -Iincludes

SRCDIR      = srcs
OBJDIR      = .objects
DEPDIR      = .deps

SRCS        = main.cpp \
              Server.cpp \
              Client.cpp \
              Channel.cpp \
              Message.cpp \
              commands/cmd_register.cpp \
              commands/cmd_join.cpp \
              commands/cmd_message.cpp \
              commands/cmd_oper.cpp \
              commands/cmd_mode.cpp

OBJS        = $(SRCS:%.cpp=$(OBJDIR)/%.o)
DEPS        = $(SRCS:%.cpp=$(DEPDIR)/%.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@) $(dir $(DEPDIR)/$*.d)
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(DEPDIR)/$*.d -c $< -o $@
# -MMD generates .d file listing the headers the .cpp file depends on
# -MP adds "dummy rules" to avoid errors if a header gets deleted
# -MF $(DEPDIR)/$*.d to write the .d file to the .deps folder instead of default location
# mkdir -p $(dir ...) so srcs/ subdirs (e.g. srcs/commands/) work without Makefile changes

-include $(DEPS)

clean:
	rm -rf $(OBJDIR) $(DEPDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
