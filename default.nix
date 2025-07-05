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
		rev = "06a53652ab8d41ab264bbe0bb1389831573f450d";
		hash = "sha256-rjilPGDJ9WyGv0Ph3orfXFyn5krCiFYqY9tSZl+9uWE=";
	} { inherit libadt; };
in
pkgs.callPackage ./build.nix { inherit libadt scallop-lang; }
