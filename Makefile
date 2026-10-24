CXX ?= g++
CXXFLAGS ?= -std=c++17 -O1 -Wall -Wextra -pedantic
BUILD ?= build

.PHONY: test test-string-stdlib test-math-stdlib test-collections-stdlib test-testing-stdlib test-std conformance all clean
all: test conformance

$(BUILD)/dslvm_test: tests/dslvm_test.cpp include/dslvm.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I include -o $@ tests/dslvm_test.cpp

$(BUILD)/conformance: conformance/run.cpp include/dslvm.hpp stdlib/string.hpp stdlib/math.hpp stdlib/collections.hpp stdlib/testing.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I include -o $@ conformance/run.cpp

test: $(BUILD)/dslvm_test
	$(BUILD)/dslvm_test
	$(MAKE) test-string-stdlib test-math-stdlib test-collections-stdlib test-testing-stdlib test-std

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

$(BUILD)/stdlib_collections_test: tests/stdlib_collections_test.cpp stdlib/collections.hpp include/dslvm.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I include -o $@ tests/stdlib_collections_test.cpp

$(BUILD)/stdlib_collections_header.o: tests/stdlib_collections_header.cpp stdlib/collections.hpp include/dslvm.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -std=c++17 -Wall -Wextra -Werror -I include -c -o $@ tests/stdlib_collections_header.cpp

test-collections-stdlib: $(BUILD)/stdlib_collections_test $(BUILD)/stdlib_collections_header.o
	$(BUILD)/stdlib_collections_test
	bash tests/string-signature-parity.sh "$(abspath $(BUILD)/stdlib_collections_test)" collections

$(BUILD)/stdlib_testing_test: tests/stdlib_testing_test.cpp stdlib/testing.hpp include/dslvm.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I include -o $@ tests/stdlib_testing_test.cpp

$(BUILD)/stdlib_testing_header.o: tests/stdlib_testing_header.cpp stdlib/testing.hpp include/dslvm.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -std=c++17 -Wall -Wextra -Werror -I include -c -o $@ tests/stdlib_testing_header.cpp

test-testing-stdlib: $(BUILD)/stdlib_testing_test $(BUILD)/stdlib_testing_header.o
	$(BUILD)/stdlib_testing_test
	bash tests/string-signature-parity.sh "$(abspath $(BUILD)/stdlib_testing_test)" testing

# Default registration pulls in the full catalog, including through standalone group headers.
$(BUILD)/dslvm_test $(BUILD)/conformance $(BUILD)/stdlib_default_test $(BUILD)/stdlib_string_test $(BUILD)/stdlib_math_test $(BUILD)/stdlib_collections_test $(BUILD)/stdlib_testing_test $(BUILD)/stdlib_string_header.o $(BUILD)/stdlib_math_header.o $(BUILD)/stdlib_collections_header.o $(BUILD)/stdlib_testing_header.o: $(wildcard stdlib/*.hpp)

$(BUILD)/stdlib_default_test: tests/stdlib_default_test.cpp include/dslvm.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I include -o $@ tests/stdlib_default_test.cpp

test-std: $(BUILD)/stdlib_default_test
	$(BUILD)/stdlib_default_test
	bash tests/string-signature-parity.sh "$(abspath $(BUILD)/stdlib_default_test)" std
	PYTHONDONTWRITEBYTECODE=1 python3 tests/test_std_schema.py
