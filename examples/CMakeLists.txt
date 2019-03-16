project (monsoon_examples)

add_executable (count count.cc)
target_link_libraries (count PRIVATE monsoon_intf monsoon_history monsoon_dirhistory)

add_executable (eval eval.cc)
target_link_libraries (eval PRIVATE monsoon_intf monsoon_expr monsoon_history monsoon_dirhistory)

add_executable (print_all_metrics print_all_metrics.cc)
target_link_libraries (print_all_metrics PRIVATE monsoon_intf monsoon_history monsoon_dirhistory)

add_executable (print_all_metric_names print_all_metric_names.cc)
target_link_libraries (print_all_metric_names PRIVATE monsoon_intf monsoon_history monsoon_dirhistory)
