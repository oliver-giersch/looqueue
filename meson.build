project('looqueue', 'cpp',
  version: '0.1.0',
  meson_version: '>=1.1',
  default_options : [
    'cpp_std=c++20'
  ]
)

if get_option('tag_high')
  add_project_arguments('-DOPTION_TAG_HIGH=1', language: 'c')
endif

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

test('unit tests', test_unit)
test('stress tests', test_stress)
