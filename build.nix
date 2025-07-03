{
	stdenv,
	cmake,
	libadt,
	scallop-lang,
}:

stdenv.mkDerivation {
	pname = "srvsh";
	version = "0.0.1";
	src = ./.;

	nativeBuildInputs = [cmake];
	buildInputs = [
		libadt
		scallop-lang
	];
}
