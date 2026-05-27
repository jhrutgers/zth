// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use regex::Regex;
use std::env;
use std::fs;
use std::path::{Path, PathBuf};

fn env_path(name: &str) -> Option<PathBuf> {
    env::var_os(name)
        .map(PathBuf::from)
        .filter(|path| path.exists())
}

fn repo_root() -> Option<PathBuf> {
    let from_cargo = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let repo_from_cargo = from_cargo.parent();
    if repo_from_cargo.is_none() {
        return None;
    }

    let repo_unwrapped = repo_from_cargo.unwrap();

    if repo_unwrapped.join("include/zth").exists() {
        return Some(repo_unwrapped.to_path_buf());
    }

    None
}

fn first_existing(paths: impl IntoIterator<Item = PathBuf>) -> Option<PathBuf> {
    paths.into_iter().find(|path| path.exists())
}

fn has_libzth(dir: &Path) -> bool {
    dir.join("libzth.a").exists() || dir.join("libzth.so").exists()
}

fn first_with_libzth(paths: impl IntoIterator<Item = PathBuf>) -> Option<PathBuf> {
    paths.into_iter().find(|path| has_libzth(path))
}

fn cmake_file_has_option(file: &Path, option: &str) -> bool {
    let Ok(contents) = fs::read_to_string(file) else {
        return false;
    };

    let cache_prefix = format!("{option}:BOOL=");
    let flag_prefix = format!("{option}:INTERNAL=");
    let set_prefix = format!("set({option}");

    contents.lines().any(|line| {
        let define = format!("{option}=1");
        if line.contains(&define) {
            return true;
        }

        let trimmed = line.trim();

        if let Some(value) = trimmed.strip_prefix(&cache_prefix) {
            return value == "ON" || value == "1";
        }

        if let Some(value) = trimmed.strip_prefix(&flag_prefix) {
            return value == "ON" || value == "1";
        }

        if !trimmed.starts_with(&set_prefix) {
            return false;
        }

        trimmed.contains(" ON") || trimmed.contains("\"ON\"") || trimmed.contains("1")
    })
}

fn cmake_file_linked(file: &Path, linked: &str) -> bool {
    let Ok(contents) = fs::read_to_string(file) else {
        return false;
    };

    let re = Regex::new(&format!(r#"_LINK_.*".*{linked}.*""#)).unwrap();
    contents.lines().any(|line| re.captures(line).is_some())
}

fn default_dist_target() -> &'static str {
    let host = env::var("HOST").unwrap_or_default();
    let host_is_windows = host.contains("-windows-");

    match env::var("CARGO_CFG_TARGET_OS").as_deref() {
        Ok("macos") => "macos",
        Ok("windows") if host_is_windows => "win32",
        Ok("windows") => "mingw",
        Ok("linux") => "ubuntu",
        _ => "ubuntu",
    }
}

fn use_libzth(
    include_dir: &PathBuf,
    lib_dir: &PathBuf,
    install_dir: &PathBuf,
    config_file: &PathBuf,
) {
    assert!(
        include_dir.exists(),
        "Zth headers not found; set ZTH_REPO or ZTH_INSTALL"
    );
    assert!(
        include_dir.join("zth").exists(),
        "Zth headers not found; set ZTH_REPO or ZTH_INSTALL"
    );
    assert!(
        lib_dir.exists(),
        "libzth not found; set ZTH_REPO or ZTH_INSTALL"
    );
    assert!(
        lib_dir.join("libzth.a").exists() || lib_dir.join("libzth.so").exists(),
        "libzth not found; set ZTH_REPO or ZTH_INSTALL"
    );
    assert!(
        config_file.exists(),
        "libzth not found; set ZTH_REPO or ZTH_INSTALL"
    );

    println!("cargo:metadata=include={}", include_dir.display());
    println!("cargo:rustc-link-search=native={}", lib_dir.display());
    if install_dir.exists() {
        println!("cargo:rustc-link-search=native={}", install_dir.display());
    }

    if cmake_file_has_option(config_file, "ZTH_ENABLE_ASAN")
        || cmake_file_linked(config_file, "-fsanitize=address")
    {
        println!("cargo:rustc-link-lib=dylib=asan");
    }

    if cmake_file_has_option(config_file, "ZTH_ENABLE_LSAN")
        || cmake_file_linked(config_file, "-fsanitize=leak")
    {
        println!("cargo:rustc-link-lib=dylib=lsan");
    }

    if cmake_file_has_option(config_file, "ZTH_ENABLE_UBSAN")
        || cmake_file_linked(config_file, "-fsanitize=undefined")
    {
        println!("cargo:rustc-link-lib=dylib=ubsan");
    }

    if cmake_file_has_option(config_file, "ZTH_HAVE_LIBZMQ")
        || cmake_file_linked(config_file, "libzmq")
    {
        println!("cargo:rustc-link-lib=dylib=zmq");
    }

    if cmake_file_has_option(config_file, "ZTH_HAVE_LIBUNWIND")
        || cmake_file_linked(config_file, "unwind")
    {
        println!("cargo:rustc-link-lib=dylib=unwind");
    }

    if cmake_file_has_option(config_file, "ZTH_HAVE_LIBBACKTRACE")
        || cmake_file_linked(config_file, "backtrace")
    {
        println!("cargo:rustc-link-lib=dylib=backtrace");
    }

    if lib_dir.join("libzth.so").exists() {
        println!("cargo:rustc-link-lib=dylib=zth");
    } else {
        println!("cargo:rustc-link-lib=static=zth");
    }

    let target_env = env::var("CARGO_CFG_TARGET_OS");
    let target = target_env.as_deref();
    if target == Ok("windows") {
        println!("cargo:rustc-link-lib=dylib=stdc++");
        println!("cargo:rustc-link-lib=dylib=pthread");
    }
    if target == Ok("linux") || target == Ok("macos") {
        println!("cargo:rustc-link-lib=dylib=stdc++");
        println!("cargo:rustc-link-lib=dylib=pthread");
        println!("cargo:rustc-link-lib=dylib=dl");
    }
    if target == Ok("linux") {
        println!("cargo:rustc-link-lib=dylib=rt");
    }

    // TODO: bare metal

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
    println!("cargo:rerun-if-changed={}", config_file.display());
}

fn build_from_repo(repo: &PathBuf, dist: &str) {
    let include_dir = repo.join("include");
    let build_dir = repo.join("dist").join(dist).join("build");
    let lib_dir = build_dir.clone();
    let install_dir = build_dir.join("deploy/lib");
    let config_file = build_dir.join("CMakeCache.txt");
    use_libzth(&include_dir, &lib_dir, &install_dir, &config_file);
}

fn build_from_install(prefix: &PathBuf) {
    let include_dir = prefix.join("include");

    let lib_dir = first_with_libzth([
        prefix.join("lib"),
        prefix.join("lib64"),
        prefix.join("lib/x86_64-linux-gnu"),
    ])
    .or_else(|| {
        first_existing([
            prefix.join("lib"),
            prefix.join("lib64"),
            prefix.join("lib/x86_64-linux-gnu"),
        ])
    })
    .unwrap_or_else(|| prefix.join("lib"));

    let config_file = first_existing([
        prefix.join("libzth/cmake/libzth.cmake"),
        prefix.join("share/libzth/cmake/libzth.cmake"),
    ])
    .unwrap_or_else(|| prefix.join("libzth/cmake/libzth.cmake"));

    use_libzth(&include_dir, &lib_dir, &lib_dir, &config_file);
}

fn main() {
    // Possible ways to build:
    //
    // 1. We are running from within the repo, where we use the built library from the
    //    dist/<target>/build dir, and the headers from ../include. Use
    //    dist/<target>/build/CMakeCache.txt for the library configuration. The target is taken from
    //    ZTH_DIST.  When it is not set, guess macos/mingw/ubuntu/win32 based on the detected
    //    platform.
    //
    // 2. We are not running from within the repo, but we use a repo that is located elsewhere. The
    //    repo can be found via ZTH_REPO. The behavior is the same as for 1.
    //
    // 3. We are not running from within the repo, and we use the installed library on the system.
    //    The install can be found via ZTH_INSTALL. ZTH_INSTALL/libzth/cmake/libzth.cmake can be
    //    parsed for the library configuration.
    //
    // To figure out where to get libzth from:
    // - If ZTH_REPO is set, use mode 2.
    // - If ZTH_INSTALL is set, use mode 3.
    // - If ../include/zth exists, use mode 1.
    // - When on Linux, if /usr/libzth/cmake/libzth.cmake exists, use mode 3 with ZTH_INSTALL=/usr.

    println!("cargo:rerun-if-env-changed=ZTH_REPO");
    println!("cargo:rerun-if-env-changed=ZTH_INSTALL");
    println!("cargo:rerun-if-env-changed=ZTH_DIST");

    println!("cargo:rustc-check-cfg=cfg(zth_hosted_std)");
    let hosted_std = env::var_os("CARGO_FEATURE_STD").is_some()
        && env::var("CARGO_CFG_TARGET_OS").as_deref() != Ok("none");
    if hosted_std {
        println!("cargo:rustc-cfg=zth_hosted_std");
    }

    let dist = env::var("ZTH_DIST").unwrap_or_else(|_| default_dist_target().to_string());

    if let Some(path) = env_path("ZTH_REPO") {
        build_from_repo(&path, &dist);
    } else if let Some(path) = env_path("ZTH_INSTALL") {
        build_from_install(&path);
    } else if let Some(path) = repo_root() {
        build_from_repo(&path, &dist);
    } else if env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("linux")
        && Path::new("/usr/libzth/cmake/libzth.cmake").exists()
    {
        build_from_install(&PathBuf::from("/usr"));
    } else {
        panic!("Cannot locate Zth. Set ZTH_REPO or ZTH_INSTALL, or run from the Zth repo.");
    }
}
