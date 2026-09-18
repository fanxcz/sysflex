# Makefile — сборка sysflex без CMake.
#
#   make            собрать sysflex (Release, -O2)
#   make test       собрать и запустить модульные тесты
#   make debug      сборка с отладочной информацией и санитайзерами
#   make install    установить в $(PREFIX) (по умолчанию /usr/local)
#   make uninstall  удалить установленные файлы
#   make clean      удалить артефакты сборки
#
# Полезные переменные: CXX, CXXSTD, OPTFLAGS, PREFIX, SYSFLEX_WERROR=1

CXX        ?= g++
CXXSTD     ?= c++17
OPTFLAGS   ?= -O2
WARNINGS   := -Wall -Wextra
ifeq ($(SYSFLEX_WERROR),1)
WARNINGS   += -Werror
endif

CXXFLAGS   ?= $(WARNINGS)
CXXFLAGS   += -std=$(CXXSTD) $(OPTFLAGS) -Iinclude -pthread
LDFLAGS    += -pthread

PREFIX     ?= /usr/local
BINDIR     := $(PREFIX)/bin
SHAREDIR   := $(PREFIX)/share/sysflex
MANDIR     := $(PREFIX)/share/man/man1

# Версия берётся из единственного источника — константы VERSION в заголовке,
# чтобы она не разъезжалась с CMakeLists.txt и --version.
VERSION    := $(shell sed -n 's/^inline const char\* VERSION = "\(.*\)";.*/\1/p' include/sysflex/config.hpp)
BIN        := sysflex
TEST_BIN   := sysflex_tests

CORE_SRC   := src/system_info.cpp src/display.cpp src/sysflex.cpp
CORE_OBJ   := $(CORE_SRC:.cpp=.o)
MAIN_OBJ   := src/main.o
TEST_OBJ   := tests/test_main.o
OBJECTS    := $(CORE_OBJ) $(MAIN_OBJ) $(TEST_OBJ)

.PHONY: all test debug install uninstall clean help

all: $(BIN)

$(BIN): $(CORE_OBJ) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_BIN): $(CORE_OBJ) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

# Сборка с санитайзерами: ловит гонки, выходы за границы и утечки
debug:
	$(MAKE) clean
	$(MAKE) OPTFLAGS="-O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer" \
	        LDFLAGS="-pthread -fsanitize=address,undefined" test

install: $(BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)
	install -d $(DESTDIR)$(SHAREDIR)/plugins
	install -m 0755 plugins/*.sh $(DESTDIR)$(SHAREDIR)/plugins/
	install -d $(DESTDIR)$(MANDIR)
	install -m 0644 man/sysflex.1 $(DESTDIR)$(MANDIR)/sysflex.1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)
	rm -rf $(DESTDIR)$(SHAREDIR)
	rm -f $(DESTDIR)$(MANDIR)/sysflex.1

clean:
	rm -f $(OBJECTS) $(OBJECTS:.o=.d) $(BIN) $(TEST_BIN)

help:
	@echo "sysflex $(VERSION)"
	@echo "  make            собрать $(BIN)"
	@echo "  make test       собрать и запустить модульные тесты"
	@echo "  make debug      сборка с AddressSanitizer/UBSan и тестами"
	@echo "  make install    установить в PREFIX=$(PREFIX)"
	@echo "  make uninstall  удалить установленные файлы"
	@echo "  make clean      удалить артефакты сборки"

-include $(OBJECTS:.o=.d)
