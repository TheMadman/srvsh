let
	pkgs = import <nixpkgs> {};
	srvsh = import ./.;
in
pkgs.mkShell {
	inputsFrom = [srvsh];
	nativeBuildInputs = [pkgs.gdb pkgs.graphviz pkgs.doxygen];
	shellHook = ''
		export CFLAGS='-Wall -Wextra -Wshadow -fsanitize=address -fsanitize=leak -fsanitize=undefined'
	'';
}
