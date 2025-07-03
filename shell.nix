let
	pkgs = import <nixpkgs> {};
	guish = import ./.;
in
pkgs.mkShell {
	inputsFrom = [guish];
	nativeBuildInputs = [pkgs.gdb pkgs.graphviz pkgs.doxygen];
	shellHook = ''
		export CFLAGS='-Wall -Wextra -Wshadow -fsanitize=address -fsanitize=leak -fsanitize=undefined'
	'';
}
