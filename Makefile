CXX ?= g++
CXXFLAGS ?= -std=c++17 -O1 -Wall -Wextra -pedantic
BUILD ?= build

.PHONY: test test-c-api test-string-stdlib test-math-stdlib test-collections-stdlib test-testing-stdlib test-file-stdlib test-std conformance all clean
all: test conformance

$(BUILD)/lmcode_c_api_test: tests/lmcode_c_api_test.c src/lmcode_c.cpp include/lmcode_c.h include/dslvm.hpp stdlib/testing.hpp
	@mkdir -p $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -I include -c tests/lmcode_c_api_test.c -o $(BUILD)/lmcode_c_api_test.c.o
	$(CXX) $(CXXFLAGS) -I include -c src/lmcode_c.cpp -o $(BUILD)/lmcode_c_api.o
	$(CXX) $(CXXFLAGS) $(BUILD)/lmcode_c_api_test.c.o $(BUILD)/lmcode_c_api.o -o $@

test-c-api: $(BUILD)/lmcode_c_api_test
	$(BUILD)/lmcode_c_api_test > $(BUILD)/lmcode_c_api.stdout
	grep -Fx '{"ok": true, "result": {"c_api":true}}' $(BUILD)/lmcode_c_api.stdout

$(BUILD)/dslvm_test: tests/dslvm_test.cpp include/dslvm.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I include -o $@ tests/dslvm_test.cpp

$(BUILD)/conformance: conformance/run.cpp tests/fixture_fs.hpp include/dslvm.hpp stdlib/string.hpp stdlib/math.hpp stdlib/collections.hpp stdlib/testing.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I include -o $@ conformance/run.cpp

test: $(BUILD)/dslvm_test
	$(BUILD)/dslvm_test
	$(MAKE) test-c-api test-string-stdlib test-math-stdlib test-collections-stdlib test-testing-stdlib test-file-stdlib test-std

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
	PYTHONDONTWRITEBYTECODE=1 python3 tests/test_std_corpus.py
	PYTHONDONTWRITEBYTECODE=1 python3 scripts/gen-std-corpus.py --check
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

$(BUILD)/stdlib_file_test: tests/stdlib_file_test.cpp tests/fixture_fs.hpp include/dslvm.hpp $(wildcard stdlib/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -I include -o $@ tests/stdlib_file_test.cpp

$(BUILD)/stdlib_file_header.o: tests/stdlib_file_header.cpp include/dslvm.hpp $(wildcard stdlib/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -std=c++17 -Wall -Wextra -Werror -I include -c -o $@ tests/stdlib_file_header.cpp

test-file-stdlib: $(BUILD)/stdlib_file_test $(BUILD)/stdlib_file_header.o
	python3 -c 'import subprocess,sys; subprocess.run([sys.argv[1]], check=True, timeout=60)' "$(abspath $(BUILD)/stdlib_file_test)"
	bash tests/string-signature-parity.sh "$(abspath $(BUILD)/stdlib_file_test)" file

# Reference host is a separate public project; CI checks out a pinned revision.
REFERENCE_HOST ?= ../lmcode-cpp-sample
.PHONY: sample-suite
sample-suite:
	$(MAKE) -C "$(REFERENCE_HOST)" SDK="$(abspath .)" SUITE="$(abspath sample-suite)" BUILD="$(abspath $(BUILD)/sample-reference)" test
	python3 -B sample-suite/test_runner.py
