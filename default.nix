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
		rev = "16ac504483e703ce3bb72fd5e21cf46d71f11f1e";
		hash = "sha256-mx7CcRnr3tetj8bOqpqhgN0VzssSu9twsMeRx+bHkWA=";
	} { inherit libadt; };
in
pkgs.callPackage ./build.nix { inherit libadt scallop-lang; }
