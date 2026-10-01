use std::env;
use std::fmt::Write;
use std::fs;
use std::path::PathBuf;

fn main() {
	let root = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
	let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
	let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap();
	let target_features = env::var("CARGO_CFG_TARGET_FEATURE").unwrap_or_default();

	let src_dir = root.join("src");
	let include_dir = root.join("include");

	let mut sources: Vec<PathBuf> = fs::read_dir(&src_dir)
		.expect("failed to read src/")
		.map(|entry| entry.expect("failed to read src/ entry").path())
		.filter(|path| path.extension().is_some_and(|ext| ext == "c"))
		.filter(|path| {
			let name = path.file_name().unwrap();
			name != "linux.c" && name != "windows.c"
		})
		.collect();
	sources.sort();

	let mut unity = String::new();
	writeln!(unity, "#include <alloc/attr.h>").unwrap();
	writeln!(unity, "#undef attr_api").unwrap();
	writeln!(unity, "#define attr_api attr_attr(visibility(\"default\")) attr_attr(externally_visible)").unwrap();
	for source in &sources {
		writeln!(unity, "#include \"{}\"", source.display()).unwrap();
	}

	let unity_path = out_dir.join("shalloc_unity.c");
	fs::write(&unity_path, unity).expect("failed to write unity source");

	let mut build = cc::Build::new();
	build
		.file(&unity_path)
		.include(&include_dir)
		.define("_GNU_SOURCE", None)
		.define("NDEBUG", None)
		.flag("-std=gnu23")
		.flag("-O3")
		.flag("-fvisibility=hidden")
		.flag("-ftls-model=initial-exec")
		.flag("-fno-semantic-interposition")
		.flag_if_supported("-fwhole-program")
		.flag("-Wno-address-of-packed-member")
		.warnings(false);

	if target_arch == "x86_64" || target_arch == "x86" {
		for (feature, flag) in [("lzcnt", "-mlzcnt"), ("bmi1", "-mbmi"), ("popcnt", "-mpopcnt")] {
			if target_features.split(',').any(|f| f == feature) {
				build.flag(flag);
			}
		}
	}

	build.compile("shalloc");

	println!("cargo:rerun-if-changed={}", src_dir.display());
	println!("cargo:rerun-if-changed={}", include_dir.display());
	println!("cargo:rerun-if-changed=rust/build.rs");
}
