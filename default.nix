let
	pkgs = import <nixpkgs> {};
	callFromGitHub = gitHubParams:
		let
			path = pkgs.fetchFromGitHub gitHubParams;
			pkg = pkgs.callPackage "${path}/build.nix";
		in
		pkg;
	libadt = callFromGitHub {
		owner = "TheMadman";
		repo = "libadt";
		rev = "b8e1fea53d4a3a120254b97f1331123a6fd5fcc9";
		hash = "sha256-cAhYJ8CU9vtgzMnO0CJUyno2v4YN3m/YURtA1pgDL2s=";
	} {};
	scallop-lang = callFromGitHub {
		owner = "TheMadman";
		repo = "scallop-lang";
		rev = "21835cfa98cba49be6374253498635d53e0ca41d";
		hash = "sha256-zDu136x9Lc2cOrncsVJhj0W69j5bMPIDm0nTPldlqD0=";
	} { inherit libadt; };
in
pkgs.callPackage ./build.nix { inherit libadt scallop-lang; }
