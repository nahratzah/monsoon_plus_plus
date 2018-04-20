project (monsoon_examples)

add_executable (count count.cc)
target_link_libraries (count PRIVATE monsoon_intf monsoon_history monsoon_dirhistory)

add_executable (eval eval.cc)
target_link_libraries (eval PRIVATE monsoon_intf monsoon_expr monsoon_history monsoon_dirhistory)
