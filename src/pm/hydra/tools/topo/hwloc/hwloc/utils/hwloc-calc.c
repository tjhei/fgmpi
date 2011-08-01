/*
 * Copyright © 2009 CNRS
 * Copyright © 2009-2011 INRIA.  All rights reserved.
 * Copyright © 2009-2011 Université Bordeaux 1
 * Copyright © 2009-2010 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 */

#include <private/private.h>
#include <hwloc-calc.h>
#include <hwloc.h>

#include "misc.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void usage(const char *callname __hwloc_attribute_unused, FILE *where)
{
  fprintf(where, "Usage: hwloc-calc [options] <location> ...\n");
  fprintf(where, " <location> may be a space-separated list of cpusets or objects\n");
  fprintf(where, "            as supported by the hwloc-bind utility.\n");
  fprintf(where, "Options:\n");
  fprintf(where, "  -l --logical              Use logical object indexes (default)\n");
  fprintf(where, "  -p --physical             Use physical object indexes\n");
  fprintf(where, "  --li --logical-input      Use logical indexes for input (default)\n");
  fprintf(where, "  --lo --logical-output     Use logical indexes for output (default)\n");
  fprintf(where, "  --pi --physical-input     Use physical indexes for input\n");
  fprintf(where, "  --po --physical-output    Use physical indexes for output\n");
  fprintf(where, "  --number-of <type|depth>\n"
                 "  -N <type|depth>           Report the number of objects intersecting the CPU set\n");
  fprintf(where, "  --intersect <type|depth>\n"
		 "  -I <type|depth>           Report the indexes of object intersecting the CPU set\n");
  fprintf(where, "  --hierarchical <type1>.<type2>...\n"
		 "  -H <type1>.<type2>...     Find the list of objects intersecting the CPU set and\n"
		 "                            display them as hierarchical combinations such as\n"
		 "                            type1:index1.type2:index2...\n");
  fprintf(where, "  --largest                 Report the list of largest objects in the CPU set\n");
  fprintf(where, "  --single                  Singlify the output to a single CPU\n");
  fprintf(where, "  --taskset                 Manipulate taskset-specific cpuset strings\n");
  hwloc_utils_input_format_usage(where, 10);
  fprintf(where, "  -v                        Show verbose messages\n");
  fprintf(where, "  --version                 Report version and exit\n");
}

static int verbose = 0;
static int logicali = 1;
static int logicalo = 1;
static int numberofdepth = -1;
static int intersectdepth = -1;
static int hiernblevels = 0;
static int *hierdepth = NULL;
static int showobjs = 0;
static int singlify = 0;
static int taskset = 0;

static void
hwloc_calc_hierarch_output(hwloc_topology_t topology, const char *prefix, hwloc_bitmap_t set, int level)
{
  hwloc_obj_t obj, prev = NULL;
  unsigned logi = 0;
  while ((obj = hwloc_get_next_obj_covering_cpuset_by_depth(topology, set, hierdepth[level], prev)) != NULL) {
    char string[256];
    char type[32];
    hwloc_obj_type_snprintf(type, sizeof(type), obj, 1);
    snprintf(string, sizeof(string), "%s%s%s:%u", prefix, level ? "." : "", type, logicalo ? logi : obj->os_index);
    if (prev)
      printf(" ");
    if (level != hiernblevels - 1) {
      hwloc_bitmap_t new = hwloc_bitmap_dup(set);
      hwloc_bitmap_and(new, new, obj->cpuset);
      hwloc_calc_hierarch_output(topology, string, new, level+1);
      hwloc_bitmap_free(new);
    } else {
      printf("%s", string);
    }
    prev = obj;
    logi++;
  }
}

static int
hwloc_calc_output(hwloc_topology_t topology, hwloc_bitmap_t set)
{
  if (singlify)
    hwloc_bitmap_singlify(set);

  if (showobjs) {
    hwloc_bitmap_t remaining = hwloc_bitmap_dup(set);
    int first = 1;
    while (!hwloc_bitmap_iszero(remaining)) {
      char type[64];
      unsigned idx;
      hwloc_obj_t obj = hwloc_get_first_largest_obj_inside_cpuset(topology, remaining);
      if (!obj) {
        fprintf(stderr, "No object included in this cpuset\n");
        return EXIT_FAILURE;
      }
      hwloc_obj_type_snprintf(type, sizeof(type), obj, 1);
      idx = logicalo ? obj->logical_index : obj->os_index;
      if (idx == (unsigned) -1)
        printf("%s%s", first ? "" : " ", type);
      else
        printf("%s%s:%u", first ? "" : " ", type, idx);
      hwloc_bitmap_andnot(remaining, remaining, obj->cpuset);
      first = 0;
    }
    printf("\n");
    hwloc_bitmap_free(remaining);
  } else if (numberofdepth != -1) {
    unsigned nb = 0;
    hwloc_obj_t obj = NULL;
    while ((obj = hwloc_get_next_obj_covering_cpuset_by_depth(topology, set, numberofdepth, obj)) != NULL)
      nb++;
    printf("%u\n", nb);
  } else if (intersectdepth != -1) {
    hwloc_obj_t proc, prev = NULL;
    while ((proc = hwloc_get_next_obj_covering_cpuset_by_depth(topology, set, intersectdepth, prev)) != NULL) {
      if (prev)
	printf(",");
      printf("%u", logicalo ? proc->logical_index : proc->os_index);
      prev = proc;
    }
    printf("\n");
  } else if (hiernblevels) {
    hwloc_calc_hierarch_output(topology, "", set, 0);
    printf("\n");
  } else {
    char *string = NULL;
    if (taskset)
      hwloc_bitmap_taskset_asprintf(&string, set);
    else
      hwloc_bitmap_asprintf(&string, set);
    printf("%s\n", string);
    free(string);
  }
  return EXIT_SUCCESS;
}

static int hwloc_calc_type_depth(const char *string, hwloc_obj_type_t *typep, int *depthp)
{
  hwloc_obj_type_t type = hwloc_obj_type_of_string(string);
  int depth = -1;
  if (type == (hwloc_obj_type_t) -1) {
    char *endptr;
    depth = strtoul(string, &endptr, 0);
    if (*endptr)
      return -1;
  }
  *depthp = depth;
  *typep = type;
  return 0;
}

static int hwloc_calc_check_type_depth(hwloc_topology_t topology, hwloc_obj_type_t type, int *depthp, const char *caller)
{
  if (type != (hwloc_obj_type_t) -1) {
    int depth = hwloc_get_type_depth(topology, type);
    if (depth == HWLOC_TYPE_DEPTH_UNKNOWN) {
      fprintf(stderr, "unavailable %s type %s\n", caller, hwloc_obj_type_string(type));
      return -1;
    } else  if (depth == HWLOC_TYPE_DEPTH_MULTIPLE) {
      fprintf(stderr, "cannot use %s type %s with multiple depth, please use the relevant depth directly\n", caller, hwloc_obj_type_string(type));
      return -1;
    }
    *depthp = depth;
  }
  return 0;
}

int main(int argc, char *argv[])
{
  hwloc_topology_t topology;
  char *input = NULL;
  enum hwloc_utils_input_format input_format = HWLOC_UTILS_INPUT_DEFAULT;
  int input_changed = 0;
  unsigned depth;
  hwloc_bitmap_t set;
  int cmdline_args = 0;
  char **orig_argv = argv;
  hwloc_obj_type_t numberoftype = (hwloc_obj_type_t) -1;
  hwloc_obj_type_t intersecttype = (hwloc_obj_type_t) -1;
  hwloc_obj_type_t *hiertype = NULL;
  char *callname;
  int opt;
  int i;
  int ret = EXIT_SUCCESS;

  callname = argv[0];

  set = hwloc_bitmap_alloc();

  hwloc_topology_init(&topology);
  hwloc_topology_load(topology);
  depth = hwloc_topology_get_depth(topology);

  while (argc >= 2) {
    if (*argv[1] == '-') {
      if (!strcmp(argv[1], "-v")) {
        verbose = 1;
        goto next;
      }
      if (!strcmp(argv[1], "--help")) {
	usage(callname, stdout);
	return EXIT_SUCCESS;
      }
      if (!strcmp(argv[1], "--number-of") || !strcmp(argv[1], "-N")) {
	if (argc <= 2) {
	  usage(callname, stderr);
	  return EXIT_SUCCESS;
	}
	if (hwloc_calc_type_depth(argv[2], &numberoftype, &numberofdepth) < 0) {
	  fprintf(stderr, "unrecognized --number-of type or depth %s\n", argv[2]);
	  usage(callname, stderr);
	  return EXIT_SUCCESS;
	}
	argv++;
	argc--;
	goto next;
      }
      if (!strcmp(argv[1], "--intersect") || !strcmp(argv[1], "-I")) {
	if (argc <= 2) {
	  usage(callname, stderr);
	  return EXIT_SUCCESS;
	}
	if (hwloc_calc_type_depth(argv[2], &intersecttype, &intersectdepth) < 0) {
	  fprintf(stderr, "unrecognized --intersect type or depth %s\n", argv[2]);
	  usage(callname, stderr);
	  return EXIT_SUCCESS;
	}
	argv++;
	argc--;
	goto next;
      }
      if (!strcmp(argv[1], "--hierarchical") || !strcmp(argv[1], "-H")) {
	char *tmp, *next;
	if (argc <= 2) {
	  usage(callname, stderr);
	  return EXIT_SUCCESS;
	}
	hiernblevels = 1;
	tmp = argv[2];
        while (1) {
	  tmp = strchr(tmp, '.');
	  if (!tmp)
	    break;
	  tmp++;
	  hiernblevels++;
        }
	hiertype = malloc(hiernblevels * sizeof(hwloc_obj_type_t));
	hierdepth = malloc(hiernblevels * sizeof(int));
	tmp = argv[2];
	for(i=0; i<hiernblevels; i++) {
	  next = strchr(tmp, '.');
	  if (next)
	    *next = '\0';
	  if (hwloc_calc_type_depth(tmp, &hiertype[i], &hierdepth[i]) < 0) {
	    fprintf(stderr, "unrecognized --hierarchical type or depth %s\n", tmp);
	    usage(callname, stderr);
	    return EXIT_SUCCESS;
	  }
	  tmp = next+1;
	}
	argv++;
	argc--;
	goto next;
      }
      if (!strcasecmp(argv[1], "--pulist") || !strcmp(argv[1], "--proclist")) {
	/* backward compat with 1.0 */
	intersecttype = HWLOC_OBJ_PU;
        goto next;
      }
      if (!strcmp(argv[1], "--nodelist")) {
	/* backward compat with 1.0 */
	intersecttype = HWLOC_OBJ_NODE;
        goto next;
      }
      if (!strcmp(argv[1], "--largest")  || !strcmp(argv[1], "--objects") /* backward compat with 1.0 */) {
	showobjs = 1;
        goto next;
      }
      if (!strcmp(argv[1], "--version")) {
        printf("%s %s\n", orig_argv[0], VERSION);
        exit(EXIT_SUCCESS);
      }
      if (!strcmp(argv[1], "-l") || !strcmp(argv[1], "--logical")) {
	logicali = 1;
	logicalo = 1;
	goto next;
      }
      if (!strcmp(argv[1], "--li") || !strcmp(argv[1], "--logical-input")) {
	logicali = 1;
	goto next;
      }
      if (!strcmp(argv[1], "--lo") || !strcmp(argv[1], "--logical-output")) {
	logicalo = 1;
	goto next;
      }
      if (!strcmp(argv[1], "-p") || !strcmp(argv[1], "--physical")) {
	logicali = 0;
	logicalo = 0;
	goto next;
      }
      if (!strcmp(argv[1], "--pi") || !strcmp(argv[1], "--physical-input")) {
	logicali = 0;
	goto next;
      }
      if (!strcmp(argv[1], "--po") || !strcmp(argv[1], "--physical-output")) {
	logicalo = 0;
	goto next;
      }
      if (!strcmp(argv[1], "--single")) {
	singlify = 1;
	goto next;
      }
      if (!strcmp(argv[1], "--taskset")) {
	taskset = 1;
	goto next;
      }
      if (hwloc_utils_lookup_input_option(argv+1, argc, &opt,
					  &input, &input_format,
					  callname)) {
	argv += opt;
	argc -= opt;
	input_changed = 1;
	goto next;
      }

      fprintf (stderr, "Unrecognized option: %s\n", argv[1]);
      usage(callname, stderr);
      return EXIT_FAILURE;
    }

    if (input_changed && input) {
      hwloc_utils_enable_input_format(topology, input, input_format, verbose, callname);
      hwloc_topology_load(topology);
      depth = hwloc_topology_get_depth(topology);
      input_changed = 0;
    }

    cmdline_args++;
    if (hwloc_mask_process_arg(topology, depth, argv[1], logicali, set, taskset, verbose) < 0)
      fprintf(stderr, "ignored unrecognized argument %s\n", argv[1]);

 next:
    argc--;
    argv++;
  }

  if (hwloc_calc_check_type_depth(topology, numberoftype, &numberofdepth, "--number-of") < 0)
    goto out;

  if (hwloc_calc_check_type_depth(topology, intersecttype, &intersectdepth, "--intersect") < 0)
    goto out;

  for(i=0; i<hiernblevels; i++)
    if (hwloc_calc_check_type_depth(topology, hiertype[i], &hierdepth[i], "--hierarchical") < 0)
      goto out;

  if (cmdline_args) {
    /* process command-line arguments */
    ret = hwloc_calc_output(topology, set);

  } else {
    /* process stdin arguments line-by-line */
#define HWLOC_CALC_LINE_LEN 1024
    char line[HWLOC_CALC_LINE_LEN];
    while (fgets(line, sizeof(line), stdin)) {
      char *current = line;
      hwloc_bitmap_zero(set);
      while (1) {
	char *token = strtok(current, " \n");
	if (!token)
	  break;
	current = NULL;
	if (hwloc_mask_process_arg(topology, depth, token, logicali, set, taskset, verbose) < 0)
	  fprintf(stderr, "ignored unrecognized argument %s\n", argv[1]);
      }
      hwloc_calc_output(topology, set);
    }
  }

 out:
  hwloc_topology_destroy(topology);

  hwloc_bitmap_free(set);

  free(hierdepth);
  free(hiertype);

  return ret;
}