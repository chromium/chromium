// Copyright 2020 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "test/ios/host/cptest_crash_view_controller.h"

@implementation CPTestCrashViewController

- (void)loadView {
  self.view = [[UIView alloc] init];

  UIStackView* buttonStack = [[UIStackView alloc] init];
  buttonStack.axis = UILayoutConstraintAxisVertical;
  buttonStack.spacing = 6;

  UIButton* button = [UIButton new];
  [button setTitle:@"UIGestureEnvironmentException"
          forState:UIControlStateNormal];
  UITapGestureRecognizer* tapGesture = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(throwUIGestureEnvironmentException)];
  [button addGestureRecognizer:tapGesture];
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];

  [buttonStack addArrangedSubview:button];

  [self.view addSubview:buttonStack];

  [buttonStack setTranslatesAutoresizingMaskIntoConstraints:NO];

  [NSLayoutConstraint activateConstraints:@[
    [buttonStack.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [buttonStack.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [buttonStack.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [buttonStack.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];
}

- (void)throwUIGestureEnvironmentException {
  NSArray* empty_array = @[];
  [empty_array objectAtIndex:42];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = UIColor.redColor;
}

@end
