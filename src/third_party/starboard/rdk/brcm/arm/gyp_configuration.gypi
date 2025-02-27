# Copyright 2020 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2016 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
{
  'includes': [
    '<(DEPTH)/starboard/sabi/sabi.gypi',
    '../../shared/libraries.gypi',
    'architecture.gypi',
  ],
  'variables': {
    'target_arch': 'arm',
    'target_os': 'linux',

    'sysroot%': '/',
    'gl_type': 'system_gles2',
    'has_ocdm%': '0',
    'enable_evergreen_lite%': '0',

    # This is to create cobalt shared library
    'final_executable_type': 'shared_library',

    'platform_libraries': [
      '<@(common_libs)',
    ],

    'linker_flags_host': [ '-pthread' ],

    # Define platform specific compiler and linker flags.
    # Refer to base.gypi for a list of all available variables.
    'compiler_flags_host': [
      '-O2',
    ],
    'compiler_flags': [
      '-fvisibility=hidden',
      # Force char to be signed.
      '-fsigned-char',
      # Disable strict aliasing.
      '-fno-strict-aliasing',

      '-fno-delete-null-pointer-checks',

      # To support large files
      '-D_FILE_OFFSET_BITS=64',

      # Suppress some warnings that will be hard to fix.
      '-Wno-unused-local-typedefs',
      '-Wno-unused-result',
      '-Wno-unused-function',
      '-Wno-deprecated-declarations',
      '-Wno-missing-field-initializers',
      '-Wno-extra',
      '-Wno-comment',  # Talk to my lawyer.
      '-Wno-narrowing',
      '-Wno-unknown-pragmas',
      '-Wno-type-limits',  # TODO: We should actually look into these.
      # It's OK not to use some input parameters. Note that the order
      # matters: Wall implies Wunused-parameter and Wno-unused-parameter
      # has no effect if specified before Wall.
      '-Wno-unused-parameter',
      '-Wno-expansion-to-defined',
      '-Wno-implicit-fallthrough',

      # Specify the sysroot with all your include dependencies.
      '--sysroot=<(sysroot)',
    ],
    'linker_flags': [
      '<@(common_linker_flags)',
      '--sysroot=<(sysroot)',
      # Cleanup unused sections
      '-Wl,-gc-sections',
      # We don't wrap these symbols, but this ensures that they aren't
      # linked in.
      # '-Wl,--wrap=malloc',
      '-Wl,--wrap=calloc',
      # '-Wl,--wrap=realloc',
      # '-Wl,--wrap=memalign',
      '-Wl,--wrap=reallocalign',
      # '-Wl,--wrap=free',
      '-Wl,--wrap=strdup',
      '-Wl,--wrap=malloc_usable_size',
      '-Wl,--wrap=malloc_stats_fast',
      '-Wl,--wrap=__cxa_demangle',
    ],
    'compiler_flags_debug': [
      '-O0',
    ],
    'compiler_flags_cc_debug': [
      '-frtti',
    ],
    'compiler_flags_devel': [
      '-O2',
    ],
    'compiler_flags_cc_devel': [
      '-frtti',
    ],
    'compiler_flags_qa': [
      '-Wno-unused-but-set-variable',
    ],
    'compiler_flags_qa_size': [
      '-Os',
    ],
    'compiler_flags_qa_speed': [
      '-O2',
    ],
    'compiler_flags_cc_qa': [
      '-frtti',
      '-fno-exceptions',
    ],
    'compiler_flags_gold': [
      '-Wno-unused-but-set-variable',
    ],
    'compiler_flags_gold_size': [
      '-Os',
    ],
    'compiler_flags_gold_speed': [
      '-O2',
    ],
    'compiler_flags_cc_gold': [
      '-frtti',
      '-fno-exceptions',
    ],
    'conditions': [
      ['cobalt_fastbuild==0', {
        'compiler_flags_debug': [
          # '-g',
        ],
        'compiler_flags_devel': [
          # '-g',
        ],
        'compiler_flags_qa': [
          '-g1',
        ],
        'compiler_flags_gold': [
          '-g1',
        ],
      }],
    ],
  },

  'target_defaults': {
    'defines': [
      # Cobalt on Linux flag
      'COBALT_LINUX',
      '__STDC_FORMAT_MACROS', # so that we get PRI*
      # Enable GNU extensions to get prototypes like ffsl.
      '_GNU_SOURCE=1',
    ],
    'cflags_c': [
      '-std=c11',
    ],
    'cflags_cc': [
      '-Wno-literal-suffix',
      '-Wno-deprecated-copy',
      '-Wno-invalid-offsetof',
      '-Wno-ignored-qualifiers',
      '-Wno-pessimizing-move',
    ],
    'default_configuration': 'rdk-brcm-arm',
    'configurations': {
      'rdk-brcm-arm_debug': {
        'inherit_from': ['debug_base'],
      },
      'rdk-brcm-arm_devel': {
        'inherit_from': ['devel_base'],
      },
      'rdk-brcm-arm_qa': {
        'inherit_from': ['qa_base'],
      },
      'rdk-brcm-arm_gold': {
        'inherit_from': ['gold_base'],
      },
    }, # end of configurations
    'target_conditions': [
      ['sb_pedantic_warnings==1', {
        'cflags': [
          '-Wall',
          '-Wextra',
          '-Wunreachable-code',
          '-Wno-maybe-uninitialized',
          # Turn warnings into errors.
          # '-Werror',
        ],
      },{
        'cflags': [
          # Do not warn for implicit type conversions that may change a value.
          '-Wno-conversion',
        ],
      }],
    ],
  }, # end of target_defaults
}
