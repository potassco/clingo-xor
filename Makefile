all:
	@cmake -G Ninja -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DCLINGOXOR_BUILD_TESTS=On -DCMAKE_BUILD_TYPE=Debug
	@cmake --build build
	@compdb -p "build" list -1 > compile_commands.json

compdb:
	compdb -p "build" list -1 > compile_commands.json

test: all
	./build/bin/test_clingo-xor

clean:
	@cmake --build build --target clean

