CXX ?= g++
CXXFLAGS ?= -std=c++17 -O1 -Wall -Wextra -pedantic
BUILD ?= build

.PHONY: test test-string-stdlib test-math-stdlib conformance all clean
all: test conformance

$(BUILD)/dslvm_test: tests/dslvm_test.cpp include/dslvm.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I include -o $@ tests/dslvm_test.cpp

$(BUILD)/conformance: conformance/run.cpp include/dslvm.hpp stdlib/string.hpp stdlib/math.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I include -o $@ conformance/run.cpp

test: $(BUILD)/dslvm_test
	$(BUILD)/dslvm_test
	$(MAKE) test-string-stdlib test-math-stdlib

$(BUILD)/stdlib_string_test: tests/stdlib_string_test.cpp stdlib/string.hpp include/dslvm.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I include -o $@ tests/stdlib_string_test.cpp

$(BUILD)/stdlib_string_header.o: tests/stdlib_string_header.cpp stdlib/string.hpp include/dslvm.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -std=c++17 -Wall -Wextra -Werror -I include -c -o $@ tests/stdlib_string_header.cpp

test-string-stdlib: $(BUILD)/stdlib_string_test $(BUILD)/stdlib_string_header.o
	$(BUILD)/stdlib_string_test
	bash tests/string-signature-parity.sh "$(abspath $(BUILD)/stdlib_string_test)"
	bash tests/string-signature-parity-test.sh

conformance: $(BUILD)/conformance
	$(BUILD)/conformance conformance/vectors
	$(BUILD)/conformance conformance/stdlib-vectors

clean:
	rm -rf $(BUILD)

$(BUILD)/stdlib_math_test: tests/stdlib_math_test.cpp stdlib/math.hpp include/dslvm.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I include -o $@ tests/stdlib_math_test.cpp

$(BUILD)/stdlib_math_header.o: tests/stdlib_math_header.cpp stdlib/math.hpp include/dslvm.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -std=c++17 -Wall -Wextra -Werror -I include -c -o $@ tests/stdlib_math_header.cpp

test-math-stdlib: $(BUILD)/stdlib_math_test $(BUILD)/stdlib_math_header.o
	$(BUILD)/stdlib_math_test
	bash tests/string-signature-parity.sh "$(abspath $(BUILD)/stdlib_math_test)" math
