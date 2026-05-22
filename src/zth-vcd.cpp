/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: MPL-2.0
 */

/*!
 * \file
 * \brief Stand-alone tool to convert perf output to VCD.
 */

#include <zth>

static void show_help(char const* prog, FILE* out = stdout)
{
	fprintf(out, "Convert Zth perf output to VCD format.\n\n");

	fprintf(out, "Usage: %s [-h|--help] [--] <perf-file> [<vcd-file>]\n\n", prog);

	fprintf(out, "Options:\n");
	fprintf(out, "  -h, --help    Show this help message and exit\n");
	fprintf(out, "Arguments:\n");
	fprintf(out, "  <perf-file>   Path to the input perf file (required)\n");
	fprintf(out,
		"  <vcd-file>    Path to the output VCD file (optional, defaults to stdout)\n");
}

int main(int argc, char* argv[])
{
	if(argc < 2) {
		show_help(argv[0], stderr);
		return 1;
	}

	char const* perf_path = nullptr;
	char const* vcd_path = nullptr;
	bool options = true;

	for(int i = 1; i < argc; ++i) {
		if(options && strcmp(argv[i], "--") == 0) {
			options = false;
		} else if(
			options && (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)) {
			show_help(argv[0]);
			return 0;
		} else if(!perf_path) {
			perf_path = argv[i];
		} else if(!vcd_path) {
			vcd_path = argv[i];
		} else {
			fprintf(stderr, "Unexpected argument: %s\n", argv[i]);
			show_help(argv[0], stderr);
			return 1;
		}
	}

	if(!perf_path) {
		fprintf(stderr, "Error: <perf-file> is required\n");
		show_help(argv[0], stderr);
		return 1;
	}

	FILE* fperf = fopen(perf_path, "rb");
	if(!fperf) {
		fprintf(stderr, "Error opening perf file '%s': %s\n", perf_path, strerror(errno));
		return 2;
	}

	FILE* fvcd = stdout;
	if(vcd_path) {
		fvcd = fopen(vcd_path, "wb");
		if(!fvcd) {
			fprintf(stderr, "Error opening VCD file '%s': %s\n", vcd_path,
				strerror(errno));
			(void)fclose(fperf);
			return 2;
		}
	}

	int res = zth::perf_vcdf(fperf, fvcd);

	(void)fclose(fperf);
	(void)fclose(fvcd);

	if(res) {
		fprintf(stderr, "Error converting perf to VCD: %s\n", strerror(res));
		return 3;
	}

	return 0;
}
