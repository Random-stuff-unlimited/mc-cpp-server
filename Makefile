NAME     = engine

# Style
GREY     = \033[0;30m
RED      = \033[0;31m
GREEN    = \033[0;32m
YELLOW   = \033[0;33m
BLUE     = \033[0;34m
PURPLE   = \033[0;35m
CYAN     = \033[0;36m
WHITE    = \033[0;37m

BOLD     = \033[1m
UNDER    = \033[4m
REV      = \033[7m
BLINK    = \033[5m

NC       = \033[0;0m
ERASE    = \033[2K\r
ERASE2   = $(ERASE)\033[F$(ERASE)

# Compiler and flags
CC       = cc

DEPFLAGS = -MMD -MP
CFLAGS   = $(DEPFLAGS) -Wall -Wextra #-Werror # -mavx # SIMD flag
LDFLAGS  = -lm 
DEBUG_FLAGS = -g3
FAST_FLAGS = -O3 -flto -march=native -mtune=native -funroll-loops -ffast-math -falign-functions=32 -falign-loops=16
# -O3 -march=native -mtune=native -flto -funsafe-math-optimizations -ffast-math -fomit-frame-pointer -funroll-loops -fno-exceptions -fno-rtti -fno-stack-protector -DNDEBUG -falign-functions=32 -falign-loops=16

ifeq ($(MAKECMDGOALS), debug)
	CFLAGS += $(DEBUG_FLAGS)
endif
ifeq ($(MAKECMDGOALS), fast)
	CC     = gcc
	CFLAGS   = $(DEPFLAGS) -Wall -Wextra
	CFLAGS += $(FAST_FLAGS)
endif

# Includes
INCLUDES = -I includes/

LIBFT_DIR = Libft
LIBFT     = $(LIBFT_DIR)/libft.a
INCLUDES += -I$(LIBFT_DIR)/include

# Source files mandatory

MAIN_DIR         = src/
MAIN_FILE        = server.c queue.c protocol.c main.c command_manager.c network_worker.c tick_manager.c network_manager.c

INIT_DIR         = src/init/
INIT_FILE        = init_mutex.c init_network.c init_server.c init_thread.c

STOP_DIR         = src/stop_server/
STOP_FILE        = clear_memory.c clear_thread.c stop_server.c


M_FILE  =   $(addprefix $(MAIN_DIR), $(MAIN_FILE)) \
			$(addprefix $(INIT_DIR), $(INIT_FILE)) \
			$(addprefix $(STOP_DIR), $(STOP_FILE))

# Object files directory
OBJ_DIR   = .obj/
OBJ       = $(M_FILE:%.c=$(OBJ_DIR)%.o)
DEPS      = $(M_FILE:%.c=$(OBJ_DIR)%.d)

COMPILED_FILES := 0

# Pattern rule for object files
$(OBJ_DIR)%.o : %.c
	@if [ $(COMPILED_FILES) -eq 0 ]; then \
		echo "\n$(YELLOW)╔══════════════════════════════════════════════╗$(NC)";                          \
		echo "$(YELLOW)║        Starting $(YELLOW2)$(NAME)$(YELLOW) compilation...        ║$(NC)";           \
		echo "$(YELLOW)╚══════════════════════════════════════════════╝$(NC)";                        \
	fi
	@$(eval COMPILED_FILES := 1)
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -o $@ -c $< $(INCLUDES)
	@printf "\n$(GREEN)[Compiling] $(NC)$(shell echo $< | sed 's|^srcs/||')";

$(NAME) : $(LIBFT) $(OBJ)
	@if [ $(COMPILED_FILES) -eq 0 ]; then \
		echo "\n$(YELLOW)╔══════════════════════════════════════════════╗$(NC)"; \
		echo "$(YELLOW)║        Starting $(YELLOW2)$(NAME)$(YELLOW) compilation...        ║$(NC)"; \
		echo "$(YELLOW)╚══════════════════════════════════════════════╝$(NC)"; \
	fi
	@$(eval COMPILED_FILES := 1)
	@echo "\n\n$(GREEN)[Compiling program] $(NC)$(NAME)"
	@$(CC) $(CFLAGS) -o $(NAME) $(OBJ) $(LIBFT) $(MINILIBX) $(LDFLAGS) $(GPU_FLAGS)
$(LIBFT):
	@echo "\n$(YELLOW)[Compiling libft]$(NC)"
	@make -C $(LIBFT_DIR)

clean :
	@echo "$(RED)[Removing] $(NC)object files"
	@rm -rf $(OBJ_DIR)

fclean : clean
	@make --no-print-directory -C $(LIBFT_DIR) fclean
	@if [ -f $(NAME) ]; then \
		echo "$(RED)[Removing] $(NC)program $(NAME)"; \
		rm -f $(NAME); \
	fi

fcleanp :
	@echo "$(RED)[Removing] $(NC)object files"
	@rm -rf $(OBJ_DIR)
	@make --no-print-directory -C $(LIBFT_DIR) fclean
	@if [ -f $(NAME) ]; then \
		echo "$(RED)[Removing] $(NC)program $(NAME)"; \
		rm -f $(NAME); \
	fi

re : fclean
	@make --no-print-directory all

fast: all

debug: all 

ffast: fcleanp
	@make --no-print-directory fast

fdebug: fcleanp
	@make --no-print-directory debug

norminette:
	@norminette src/ includes/

.PHONY: all clean fclean nothing_to_be_done re fast ffast debug fdebug fcleanp

-include $(DEPS)
