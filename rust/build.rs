// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use std::env;
use std::fs;
use std::path::{Path, PathBuf};

fn env_path(name: &str) -> Option<PathBuf> {
    env::var_os(name)
        .map(PathBuf::from)
        .filter(|path| path.exists())
}

fn repo_root() -> PathBuf {
    if let Some(path) = env_path("ZTH_REPO_ROOT") {
        return path;
    }

    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .expect("rust crate is expected to live directly under the repository root")
        .to_path_buf()
}

fn first_existing(paths: impl IntoIterator<Item = PathBuf>) -> Option<PathBuf> {
    paths.into_iter().find(|path| path.exists())
}

fn cmake_cache_has_option(cache: &Path, option: &str) -> bool {
    let Ok(contents) = fs::read_to_string(cache) else {
        return false;
    };

    let prefix = format!("{option}:BOOL=");
    contents
        .lines()
        .find_map(|line| line.strip_prefix(&prefix))
        .is_some_and(|value| value == "ON")
}

fn link_sanitizers(cache: &Path) {
    if cmake_cache_has_option(cache, "ZTH_ENABLE_ASAN") {
        println!("cargo:rustc-link-lib=dylib=asan");
    }

    if cmake_cache_has_option(cache, "ZTH_ENABLE_UBSAN") {
        println!("cargo:rustc-link-lib=dylib=ubsan");
    }
}

fn main() {
    println!("cargo:rerun-if-env-changed=ZTH_INCLUDE_DIR");
    println!("cargo:rerun-if-env-changed=ZTH_LIB_DIR");
    println!("cargo:rerun-if-env-changed=ZTH_REPO_ROOT");

    let repo_root = repo_root();
    let include_dir = env_path("ZTH_INCLUDE_DIR").unwrap_or_else(|| repo_root.join("include"));
    let lib_dir = env_path("ZTH_LIB_DIR").or_else(|| {
        first_existing([
            repo_root.join("dist/ubuntu/build"),
            repo_root.join("dist/ubuntu/build/lib"),
        ])
    });

    assert!(
        include_dir.exists(),
        "Zth headers not found; set ZTH_INCLUDE_DIR to the directory that contains zth.h"
    );

    let lib_dir = lib_dir.expect(
        "libzth not found; set ZTH_LIB_DIR to the directory that contains libzth.a or libzth.so",
    );

    println!("cargo:metadata=include={}", include_dir.display());
    println!("cargo:rustc-link-search=native={}", lib_dir.display());
    println!("cargo:rustc-link-lib=static=zth");
    println!("cargo:rustc-link-lib=dylib=stdc++");
    println!("cargo:rustc-link-lib=dylib=pthread");
    println!("cargo:rustc-link-lib=dylib=dl");
    println!("cargo:rustc-link-lib=dylib=rt");

    let cmake_cache = repo_root.join("dist/ubuntu/build/CMakeCache.txt");
    link_sanitizers(&cmake_cache);

    if cmake_cache_has_option(&cmake_cache, "ZTH_HAVE_LIBZMQ") {
        println!("cargo:rustc-link-lib=dylib=zmq");
    }

    println!(
        "cargo:rerun-if-changed={}",
        include_dir.join("zth.h").display()
    );
    for library in ["libzth.a", "libzth.so"] {
        let library = lib_dir.join(library);
        if library.exists() {
            println!("cargo:rerun-if-changed={}", library.display());
        }
    }
    println!("cargo:rerun-if-changed={}", cmake_cache.display());
}
