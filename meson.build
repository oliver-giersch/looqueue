project('looqueue', 'cpp',
  default_options : [
    'cpp_std=c++20'
  ]
)

# header-only library
looqueue = declare_dependency(
  include_directories : include_directories('include')
)

threads = dependency('threads')

test = executable(
  'test',
  'test/test.cpp',
  dependencies : [threads, looqueue]
)

test_stress = executable(
  'test_stress',
  'test/test_stress.cpp',
  dependencies : [threads, looqueue]
)
