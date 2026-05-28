// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use proc_macro::TokenStream;
use quote::{format_ident, quote};
use syn::{parse_macro_input, Error, ItemFn};

fn validate_signature(kind: &str, input: &ItemFn) -> Result<(), Error> {
    if !input.sig.inputs.is_empty() {
        return Err(Error::new_spanned(
            &input.sig,
            format!("#[zth::{kind}] requires a zero-argument function"),
        ));
    }

    if input.sig.asyncness.is_some() {
        return Err(Error::new_spanned(
            &input.sig,
            format!("#[zth::{kind}] does not support async fn"),
        ));
    }

    if input.sig.constness.is_some() {
        return Err(Error::new_spanned(
            &input.sig,
            format!("#[zth::{kind}] does not support const fn"),
        ));
    }

    if input.sig.abi.is_some() {
        return Err(Error::new_spanned(
            &input.sig,
            format!("#[zth::{kind}] expects a Rust ABI function"),
        ));
    }

    Ok(())
}

fn rename_user_fn(mut input: ItemFn) -> (ItemFn, syn::Ident) {
    let original = input.sig.ident.clone();
    let renamed = format_ident!("__zth_user_{}", original);
    input.sig.ident = renamed.clone();
    (input, renamed)
}

/// Use the annotated function as `main()` entry point of the application.
///
/// Also use `#![no_main]` in your application. This gives a consistent entry point, regardless of
/// whether the `std` feature is used or not.
#[proc_macro_attribute]
pub fn main(_attr: TokenStream, item: TokenStream) -> TokenStream {
    let input = parse_macro_input!(item as ItemFn);
    if let Err(err) = validate_signature("main", &input) {
        return err.to_compile_error().into();
    }

    let (user_fn, user_name) = rename_user_fn(input);

    let expanded = quote! {
        #user_fn

        #[cfg(any(feature = "std", not(target_os = "none")))]
        fn main() {
            let __zth_rc: ::core::ffi::c_int = zth::__private::to_exit_code(#user_name());
            if __zth_rc != 0 {
                ::std::process::exit(__zth_rc);
            }
        }

        #[cfg(any(not(feature = "std"), target_os = "none"))]
        #[unsafe(no_mangle)]
        pub extern "C" fn main(
            _argc: ::core::ffi::c_int,
            _argv: *mut *mut ::core::ffi::c_char,
        ) -> ::core::ffi::c_int {
            zth::__private::to_exit_code(#user_name())
        }
    };

    expanded.into()
}

/// Use the annotated function as main fiber of the application.
///
/// Also use `#![no_main]` in your application. It provides a `main()` function, that uses
/// `zth::run` to execute the given fiber function.
#[proc_macro_attribute]
pub fn main_fiber(_attr: TokenStream, item: TokenStream) -> TokenStream {
    let input = parse_macro_input!(item as ItemFn);
    if let Err(err) = validate_signature("main_fiber", &input) {
        return err.to_compile_error().into();
    }

    let (user_fn, user_name) = rename_user_fn(input);

    let expanded = quote! {
        #user_fn

        #[cfg(any(feature = "std", not(target_os = "none")))]
        fn main() {
            ::std::process::exit(
            match zth::run(#user_name, ()) {
                ::core::result::Result::Ok(value) => zth::__private::to_exit_code(value),
                ::core::result::Result::Err(error) => 2,
            })
        }

        #[cfg(any(not(feature = "std"), target_os = "none"))]
        #[unsafe(no_mangle)]
        pub extern "C" fn main(
            _argc: ::core::ffi::c_int,
            _argv: *mut *mut ::core::ffi::c_char,
        ) -> ::core::ffi::c_int {
            match zth::run(#user_name, ()) {
                ::core::result::Result::Ok(value) => zth::__private::to_exit_code(value),
                ::core::result::Result::Err(error) => 2,
            }
        }
    };

    expanded.into()
}
