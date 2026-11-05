project('looqueue', 'cpp',
  default_options : [
    'cpp_std=c++20'
  ]
)

threads = dependency('threads')

# header-only library
looqueue = declare_dependency(
  include_directories : include_directories('include')
)

test_unit = executable(
  'test_unit',
  'test/test.cpp',
  dependencies : [threads, looqueue]
)

test_stress = executable(
  'test_stress',
  'test/test_stress.cpp',
  dependencies : [threads, looqueue]
)

test('Unit tests', test_unit)
test('Stress tests', test_stress)