/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <fluent-bit.h>
#include <fluent-bit/flb_time.h>
#include "flb_tests_runtime.h"

pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;
int  num_output = 0;

static int cb_count_metrics_msgpack(void *record, size_t size, void *data)
{
    int i;
    int ret;
    size_t off = 0;
    cfl_sds_t text = NULL;
    struct cmt *cmt = NULL;
    char *p;

    if (!TEST_CHECK(data != NULL)) {
        flb_error("data is NULL");
    }

    /* get cmetrics context */
    ret = cmt_decode_msgpack_create(&cmt, (char *) record, size, &off);
    if (ret != 0) {
        flb_error("could not process metrics payload");
        return -1;
    }

    /* convert to text representation */
    text = cmt_encode_text_create(cmt);
    /* To inspect the metrics from the callback, just comment out below: */
    /* flb_info("[filter_grep][test] text = %s", text); */
    for (i = 0; i < strlen(text); i++) {
        p = (char *)(text + i);
        if (*p == '\n') {
            num_output++;
        }
    }

    if (record) {
        flb_free(record);
    }

    /* destroy cmt context */
    cmt_destroy(cmt);

    cmt_encode_text_destroy(text);

    return 0;
}


static void clear_output_num()
{
    pthread_mutex_lock(&result_mutex);
    num_output = 0;
    pthread_mutex_unlock(&result_mutex);
}

static int get_output_num()
{
    int ret;
    pthread_mutex_lock(&result_mutex);
    ret = num_output;
    pthread_mutex_unlock(&result_mutex);

    return ret;
}

#ifdef FLB_HAVE_METRICS
void flb_test_selector_regex(void)
{
    int ret;
    flb_ctx_t *ctx;
    int in_ffd;
    int out_ffd;
    struct flb_processor *proc;
    struct flb_processor_unit *pu;
    struct cfl_variant var = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "storage",
    };

    ctx = flb_create();
    flb_service_set(ctx,
                    "Flush", "0.500000000",
                    "Grace", "1",
                    NULL);

    proc = flb_processor_create(ctx->config, "unit_test", NULL, 0);
    TEST_CHECK(proc != NULL);

    pu = flb_processor_unit_create(proc, FLB_PROCESSOR_METRICS, "selector");
    TEST_CHECK(pu != NULL);
    ret = flb_processor_unit_set_property(pu, "metrics.regex", &var);
    TEST_CHECK(ret == 0);


    /* Input */
    in_ffd = flb_input(ctx, (char *) "fluentbit_metrics", NULL);
    TEST_CHECK(in_ffd >= 0);
    ret = flb_input_set(ctx, in_ffd, "tag", "test", NULL);
    TEST_CHECK(ret == 0);
    ret = flb_input_set(ctx, in_ffd, "scrape_on_start", "true", NULL);
    TEST_CHECK(ret == 0);
    ret = flb_input_set(ctx, in_ffd, "scrape_interval", "1", NULL);
    TEST_CHECK(ret == 0);

    /* set up processor */
    ret = flb_input_set_processor(ctx, in_ffd, proc);
    TEST_CHECK(ret == 0);

    out_ffd = flb_output(ctx, (char *) "stdout", NULL);
    TEST_CHECK(out_ffd >= 0);
    flb_output_set(ctx, out_ffd, "match", "test", NULL);

    ret = flb_start(ctx);
    TEST_CHECK(ret == 0);

    flb_time_msleep(1500); /* waiting flush */

    flb_stop(ctx);
    flb_destroy(ctx);
}

void flb_test_selector_exclude(void)
{
    int ret;
    flb_ctx_t *ctx;
    int in_ffd;
    int out_ffd;
    struct flb_processor *proc;
    struct flb_processor_unit *pu;
    struct cfl_variant var = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "input",
    };

    ctx = flb_create();
    flb_service_set(ctx,
                    "Flush", "0.500000000",
                    "Grace", "1",
                    NULL);

    proc = flb_processor_create(ctx->config, "unit_test", NULL, 0);
    TEST_CHECK(proc != NULL);

    pu = flb_processor_unit_create(proc, FLB_PROCESSOR_METRICS, "selector");
    TEST_CHECK(pu != NULL);
    ret = flb_processor_unit_set_property(pu, "metrics.exclude", &var);
    TEST_CHECK(ret == 0);


    /* Input */
    in_ffd = flb_input(ctx, (char *) "fluentbit_metrics", NULL);
    TEST_CHECK(in_ffd >= 0);
    ret = flb_input_set(ctx, in_ffd, "tag", "test", NULL);
    TEST_CHECK(ret == 0);
    ret = flb_input_set(ctx, in_ffd, "scrape_on_start", "true", NULL);
    TEST_CHECK(ret == 0);
    ret = flb_input_set(ctx, in_ffd, "scrape_interval", "1", NULL);
    TEST_CHECK(ret == 0);

    /* set up processor */
    ret = flb_input_set_processor(ctx, in_ffd, proc);
    TEST_CHECK(ret == 0);

    out_ffd = flb_output(ctx, (char *) "stdout", NULL);
    TEST_CHECK(out_ffd >= 0);
    flb_output_set(ctx, out_ffd, "match", "test", NULL);

    ret = flb_start(ctx);
    TEST_CHECK(ret == 0);

    flb_time_msleep(1500); /* waiting flush */

    flb_stop(ctx);
    flb_destroy(ctx);
}

void flb_test_selector_multi_regex(void)
{
    int ret;
    flb_ctx_t *ctx;
    int in_ffd;
    int out_ffd;
    struct flb_processor *proc;
    struct flb_processor_unit *pu;
    struct cfl_variant var1 = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "input",
    };
    struct cfl_variant var2 = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "busy",
    };
    int got;
    int n_metrics = 2;
    int not_used = 0;
    struct flb_lib_out_cb cb_data;

    /* Prepare output callback with expected result */
    cb_data.cb = cb_count_metrics_msgpack;
    cb_data.data = &not_used;

    ctx = flb_create();
    flb_service_set(ctx,
                    "Flush", "0.500000000",
                    "Grace", "1",
                    NULL);

    proc = flb_processor_create(ctx->config, "unit_test", NULL, 0);
    TEST_CHECK(proc != NULL);

    pu = flb_processor_unit_create(proc, FLB_PROCESSOR_METRICS, "selector");
    TEST_CHECK(pu != NULL);
    ret = flb_processor_unit_set_property(pu, "metrics.regex", &var1);
    TEST_CHECK(ret == 0);
    ret = flb_processor_unit_set_property(pu, "metrics.regex", &var2);
    TEST_CHECK(ret == 0);


    /* Input */
    in_ffd = flb_input(ctx, (char *) "fluentbit_metrics", NULL);
    TEST_CHECK(in_ffd >= 0);
    ret = flb_input_set(ctx, in_ffd, "tag", "test", NULL);
    TEST_CHECK(ret == 0);
    ret = flb_input_set(ctx, in_ffd, "scrape_on_start", "true", NULL);
    TEST_CHECK(ret == 0);
    ret = flb_input_set(ctx, in_ffd, "scrape_interval", "1", NULL);
    TEST_CHECK(ret == 0);

    /* set up processor */
    ret = flb_input_set_processor(ctx, in_ffd, proc);
    TEST_CHECK(ret == 0);

    out_ffd = flb_output(ctx, (char *) "lib", &cb_data);
    TEST_CHECK(out_ffd >= 0);
    flb_output_set(ctx, out_ffd, "match", "test", NULL);

    clear_output_num();

    ret = flb_start(ctx);
    TEST_CHECK(ret == 0);

    flb_time_msleep(1500); /* waiting flush */

    got = get_output_num();
    if (!TEST_CHECK(got >= n_metrics)) {
        TEST_MSG("expect: %d >= %d, got: %d < %d", got, n_metrics, got, n_metrics);
    }

    flb_stop(ctx);
    flb_destroy(ctx);
}

void flb_test_selector_multi_exclude(void)
{
    int ret;
    flb_ctx_t *ctx;
    int in_ffd;
    int out_ffd;
    struct flb_processor *proc;
    struct flb_processor_unit *pu;
    struct cfl_variant var1 = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "input",
    };
    struct cfl_variant var2 = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "busy",
    };
    int got;
    int n_metrics = 18;
    int not_used = 0;
    struct flb_lib_out_cb cb_data;

    /* Prepare output callback with expected result */
    cb_data.cb = cb_count_metrics_msgpack;
    cb_data.data = &not_used;

    ctx = flb_create();
    flb_service_set(ctx,
                    "Flush", "0.500000000",
                    "Grace", "1",
                    NULL);

    proc = flb_processor_create(ctx->config, "unit_test", NULL, 0);
    TEST_CHECK(proc != NULL);

    pu = flb_processor_unit_create(proc, FLB_PROCESSOR_METRICS, "selector");
    TEST_CHECK(pu != NULL);
    ret = flb_processor_unit_set_property(pu, "metrics.exclude", &var1);
    TEST_CHECK(ret == 0);
    ret = flb_processor_unit_set_property(pu, "metrics.exclude", &var2);
    TEST_CHECK(ret == 0);


    /* Input */
    in_ffd = flb_input(ctx, (char *) "fluentbit_metrics", NULL);
    TEST_CHECK(in_ffd >= 0);
    ret = flb_input_set(ctx, in_ffd, "tag", "test", NULL);
    TEST_CHECK(ret == 0);
    ret = flb_input_set(ctx, in_ffd, "scrape_on_start", "true", NULL);
    TEST_CHECK(ret == 0);
    ret = flb_input_set(ctx, in_ffd, "scrape_interval", "1", NULL);
    TEST_CHECK(ret == 0);

    /* set up processor */
    ret = flb_input_set_processor(ctx, in_ffd, proc);
    TEST_CHECK(ret == 0);

    out_ffd = flb_output(ctx, (char *) "lib", &cb_data);
    TEST_CHECK(out_ffd >= 0);
    flb_output_set(ctx, out_ffd, "match", "test", NULL);

    clear_output_num();

    ret = flb_start(ctx);
    TEST_CHECK(ret == 0);

    flb_time_msleep(1500); /* waiting flush */

    got = get_output_num();
    if (!TEST_CHECK(got >= n_metrics)) {
        TEST_MSG("expect: %d >= %d, got: %d < %d", got, n_metrics, got, n_metrics);
    }

    flb_stop(ctx);
    flb_destroy(ctx);
}

void flb_test_selector_error_AND_regex_exclude(void)
{
    int ret;
    struct flb_processor *proc;
    struct flb_processor_unit *pu;
    struct flb_config *config;
    struct cfl_variant var1 = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "input",
    };
    struct cfl_variant var2 = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "busy",
    };
    struct cfl_variant op = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "AND",
    };

    flb_init_env();

    config = flb_config_init();

    proc = flb_processor_create(config, "unit_test", NULL, 0);
    TEST_CHECK(proc != NULL);

    pu = flb_processor_unit_create(proc, FLB_PROCESSOR_METRICS, "selector");
    TEST_CHECK(pu != NULL);
    ret = flb_processor_unit_set_property(pu, "metrics.regex", &var1);
    TEST_CHECK(ret == 0);
    ret = flb_processor_unit_set_property(pu, "metrics.exclude", &var2);
    TEST_CHECK(ret == 0);
    ret = flb_processor_unit_set_property(pu, "logical_op", &op);
    TEST_CHECK(ret == 0);

    ret = flb_processor_init(proc);
    TEST_CHECK(ret != 0);

    flb_processor_destroy(proc);
    flb_config_exit(config);
}

void flb_test_selector_error_OR_regex_exclude(void)
{
    int ret;
    struct flb_processor *proc;
    struct flb_processor_unit *pu;
    struct flb_config *config;
    struct cfl_variant var1 = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "input",
    };
    struct cfl_variant var2 = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "busy",
    };
    struct cfl_variant op = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "OR",
    };

    flb_init_env();

    config = flb_config_init();

    proc = flb_processor_create(config, "unit_test", NULL, 0);
    TEST_CHECK(proc != NULL);

    pu = flb_processor_unit_create(proc, FLB_PROCESSOR_METRICS, "selector");
    TEST_CHECK(pu != NULL);
    ret = flb_processor_unit_set_property(pu, "metrics.regex", &var1);
    TEST_CHECK(ret == 0);
    ret = flb_processor_unit_set_property(pu, "metrics.exclude", &var2);
    TEST_CHECK(ret == 0);
    ret = flb_processor_unit_set_property(pu, "logical_op", &op);
    TEST_CHECK(ret == 0);

    ret = flb_processor_init(proc);
    TEST_CHECK(ret != 0);

    flb_processor_destroy(proc);
    flb_config_exit(config);
}

void flb_test_selector_AND_regex(void)
{
    int ret;
    flb_ctx_t *ctx;
    int in_ffd;
    int out_ffd;
    struct flb_processor *proc;
    struct flb_processor_unit *pu;
    struct cfl_variant var1 = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "input",
    };
    struct cfl_variant var2 = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "busy",
    };
    int got;
    int n_metrics = 2;
    int not_used = 0;
    struct flb_lib_out_cb cb_data;

    /* Prepare output callback with expected result */
    cb_data.cb = cb_count_metrics_msgpack;
    cb_data.data = &not_used;

    ctx = flb_create();
    flb_service_set(ctx,
                    "Flush", "0.500000000",
                    "Grace", "1",
                    NULL);

    proc = flb_processor_create(ctx->config, "unit_test", NULL, 0);
    TEST_CHECK(proc != NULL);

    pu = flb_processor_unit_create(proc, FLB_PROCESSOR_METRICS, "selector");
    TEST_CHECK(pu != NULL);
    ret = flb_processor_unit_set_property(pu, "metrics.regex", &var1);
    TEST_CHECK(ret == 0);
    ret = flb_processor_unit_set_property(pu, "metrics.regex", &var2);
    TEST_CHECK(ret == 0);


    /* Input */
    in_ffd = flb_input(ctx, (char *) "fluentbit_metrics", NULL);
    TEST_CHECK(in_ffd >= 0);
    ret = flb_input_set(ctx, in_ffd, "tag", "test", NULL);
    TEST_CHECK(ret == 0);
    ret = flb_input_set(ctx, in_ffd, "scrape_on_start", "true", NULL);
    TEST_CHECK(ret == 0);
    ret = flb_input_set(ctx, in_ffd, "scrape_interval", "1", NULL);
    TEST_CHECK(ret == 0);

    /* set up processor */
    ret = flb_input_set_processor(ctx, in_ffd, proc);
    TEST_CHECK(ret == 0);

    out_ffd = flb_output(ctx, (char *) "lib", &cb_data);
    TEST_CHECK(out_ffd >= 0);
    flb_output_set(ctx, out_ffd, "match", "test", NULL);

    clear_output_num();

    ret = flb_start(ctx);
    TEST_CHECK(ret == 0);

    flb_time_msleep(1500); /* waiting flush */

    got = get_output_num();
    if (!TEST_CHECK(got >= n_metrics)) {
        TEST_MSG("expect: %d >= %d, got: %d < %d", got, n_metrics, got, n_metrics);
    }

    flb_stop(ctx);
    flb_destroy(ctx);
}

void flb_test_selector_OR_regex(void)
{
    int ret;
    flb_ctx_t *ctx;
    int in_ffd;
    int out_ffd;
    struct flb_processor *proc;
    struct flb_processor_unit *pu;
    struct cfl_variant var1 = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "chunk",
    };
    struct cfl_variant var2 = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "busy",
    };
    struct cfl_variant or = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "OR",
    };
    int got;
    int n_metrics = 14;
    int not_used = 0;
    struct flb_lib_out_cb cb_data;

    /* Prepare output callback with expected result */
    cb_data.cb = cb_count_metrics_msgpack;
    cb_data.data = &not_used;

    ctx = flb_create();
    flb_service_set(ctx,
                    "Flush", "0.500000000",
                    "Grace", "1",
                    NULL);

    proc = flb_processor_create(ctx->config, "unit_test", NULL, 0);
    TEST_CHECK(proc != NULL);

    pu = flb_processor_unit_create(proc, FLB_PROCESSOR_METRICS, "selector");
    TEST_CHECK(pu != NULL);
    ret = flb_processor_unit_set_property(pu, "metrics.regex", &var1);
    TEST_CHECK(ret == 0);
    ret = flb_processor_unit_set_property(pu, "metrics.regex", &var2);
    TEST_CHECK(ret == 0);
    ret = flb_processor_unit_set_property(pu, "logical_op", &or);
    TEST_CHECK(ret == 0);


    /* Input */
    in_ffd = flb_input(ctx, (char *) "fluentbit_metrics", NULL);
    TEST_CHECK(in_ffd >= 0);
    ret = flb_input_set(ctx, in_ffd, "tag", "test", NULL);
    TEST_CHECK(ret == 0);
    ret = flb_input_set(ctx, in_ffd, "scrape_on_start", "true", NULL);
    TEST_CHECK(ret == 0);
    ret = flb_input_set(ctx, in_ffd, "scrape_interval", "1", NULL);
    TEST_CHECK(ret == 0);

    /* set up processor */
    ret = flb_input_set_processor(ctx, in_ffd, proc);
    TEST_CHECK(ret == 0);

    out_ffd = flb_output(ctx, (char *) "lib", &cb_data);
    TEST_CHECK(out_ffd >= 0);
    flb_output_set(ctx, out_ffd, "match", "test", NULL);

    clear_output_num();

    ret = flb_start(ctx);
    TEST_CHECK(ret == 0);

    flb_time_msleep(1500); /* waiting flush */

    got = get_output_num();
    if (!TEST_CHECK(got >= n_metrics)) {
        TEST_MSG("expect: %d >= %d, got: %d < %d", got, n_metrics, got, n_metrics);
    }

    flb_stop(ctx);
    flb_destroy(ctx);
}

void flb_test_selector_AND_exclude(void)
{
    int ret;
    flb_ctx_t *ctx;
    int in_ffd;
    int out_ffd;
    struct flb_processor *proc;
    struct flb_processor_unit *pu;
    struct cfl_variant var1 = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "filter",
    };
    struct cfl_variant var2 = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "input",
    };
    struct cfl_variant and = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "AND",
    };
    int got;
    int n_metrics = 17;
    int not_used = 0;
    struct flb_lib_out_cb cb_data;

    /* Prepare output callback with expected result */
    cb_data.cb = cb_count_metrics_msgpack;
    cb_data.data = &not_used;

    ctx = flb_create();
    flb_service_set(ctx,
                    "Flush", "0.500000000",
                    "Grace", "1",
                    NULL);

    proc = flb_processor_create(ctx->config, "unit_test", NULL, 0);
    TEST_CHECK(proc != NULL);

    pu = flb_processor_unit_create(proc, FLB_PROCESSOR_METRICS, "selector");
    TEST_CHECK(pu != NULL);
    ret = flb_processor_unit_set_property(pu, "metrics.exclude", &var1);
    TEST_CHECK(ret == 0);
    ret = flb_processor_unit_set_property(pu, "metrics.exclude", &var2);
    TEST_CHECK(ret == 0);
    ret = flb_processor_unit_set_property(pu, "logical_op", &and);
    TEST_CHECK(ret == 0);


    /* Input */
    in_ffd = flb_input(ctx, (char *) "fluentbit_metrics", NULL);
    TEST_CHECK(in_ffd >= 0);
    ret = flb_input_set(ctx, in_ffd, "tag", "test", NULL);
    TEST_CHECK(ret == 0);
    ret = flb_input_set(ctx, in_ffd, "scrape_on_start", "true", NULL);
    TEST_CHECK(ret == 0);
    ret = flb_input_set(ctx, in_ffd, "scrape_interval", "1", NULL);
    TEST_CHECK(ret == 0);

    /* set up processor */
    ret = flb_input_set_processor(ctx, in_ffd, proc);
    TEST_CHECK(ret == 0);

    out_ffd = flb_output(ctx, (char *) "lib", &cb_data);
    TEST_CHECK(out_ffd >= 0);
    flb_output_set(ctx, out_ffd, "match", "test", NULL);

    clear_output_num();

    ret = flb_start(ctx);
    TEST_CHECK(ret == 0);

    flb_time_msleep(1500); /* waiting flush */

    got = get_output_num();
    if (!TEST_CHECK(got >= n_metrics)) {
        TEST_MSG("expect: %d >= %d, got: %d < %d", got, n_metrics, got, n_metrics);
    }

    flb_stop(ctx);
    flb_destroy(ctx);
}

void flb_test_selector_OR_exclude(void)
{
    int ret;
    flb_ctx_t *ctx;
    int in_ffd;
    int out_ffd;
    struct flb_processor *proc;
    struct flb_processor_unit *pu;
    struct cfl_variant var1 = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "fluentbit",
    };
    struct cfl_variant var2 = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "storage",
    };
    struct cfl_variant or = {
        .type = CFL_VARIANT_STRING,
        .data.as_string = "OR",
    };
    int got;
    int n_metrics = 15;
    int not_used = 0;
    struct flb_lib_out_cb cb_data;

    /* Prepare output callback with expected result */
    cb_data.cb = cb_count_metrics_msgpack;
    cb_data.data = &not_used;

    ctx = flb_create();
    flb_service_set(ctx,
                    "Flush", "0.500000000",
                    "Grace", "1",
                    NULL);

    proc = flb_processor_create(ctx->config, "unit_test", NULL, 0);
    TEST_CHECK(proc != NULL);

    pu = flb_processor_unit_create(proc, FLB_PROCESSOR_METRICS, "selector");
    TEST_CHECK(pu != NULL);
    ret = flb_processor_unit_set_property(pu, "metrics.exclude", &var1);
    TEST_CHECK(ret == 0);
    ret = flb_processor_unit_set_property(pu, "metrics.exclude", &var2);
    TEST_CHECK(ret == 0);
    ret = flb_processor_unit_set_property(pu, "logical_op", &or);
    TEST_CHECK(ret == 0);


    /* Input */
    in_ffd = flb_input(ctx, (char *) "fluentbit_metrics", NULL);
    TEST_CHECK(in_ffd >= 0);
    ret = flb_input_set(ctx, in_ffd, "tag", "test", NULL);
    TEST_CHECK(ret == 0);
    ret = flb_input_set(ctx, in_ffd, "scrape_on_start", "true", NULL);
    TEST_CHECK(ret == 0);
    ret = flb_input_set(ctx, in_ffd, "scrape_interval", "1", NULL);
    TEST_CHECK(ret == 0);

    /* set up processor */
    ret = flb_input_set_processor(ctx, in_ffd, proc);
    TEST_CHECK(ret == 0);

    out_ffd = flb_output(ctx, (char *) "lib", &cb_data);
    TEST_CHECK(out_ffd >= 0);
    flb_output_set(ctx, out_ffd, "match", "test", NULL);

    clear_output_num();

    ret = flb_start(ctx);
    TEST_CHECK(ret == 0);

    flb_time_msleep(1500); /* waiting flush */

    got = get_output_num();
    if (!TEST_CHECK(got >= n_metrics)) {
        TEST_MSG("expect: %d >= %d, got: %d < %d", got, n_metrics, got, n_metrics);
    }

    flb_stop(ctx);
    flb_destroy(ctx);
}
#endif

/* Test list */
TEST_LIST = {
#ifdef FLB_HAVE_METRICS
    {"regex", flb_test_selector_regex},
    {"exclude", flb_test_selector_exclude},
    {"multi_regex", flb_test_selector_multi_regex},
    {"multi_exclude", flb_test_selector_multi_exclude},
    {"error_AND_regex_exclude", flb_test_selector_error_AND_regex_exclude},
    {"error_OR_regex_exclude", flb_test_selector_error_OR_regex_exclude},
    {"AND_regex", flb_test_selector_AND_regex},
    {"OR_regex", flb_test_selector_OR_regex},
    {"AND_exclude", flb_test_selector_AND_exclude},
    {"OR_exclude", flb_test_selector_OR_exclude},
#endif
    {NULL, NULL}
};
