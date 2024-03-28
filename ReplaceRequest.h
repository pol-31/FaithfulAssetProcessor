#ifndef FAITHFUL_UTILS_ASSETPROCESSOR_REPLACEREQUEST_H_
#define FAITHFUL_UTILS_ASSETPROCESSOR_REPLACEREQUEST_H_

#include <iostream>
#include <string>

/// simple functor for replace requesting with "force" flags,
/// designed to have shared "force" flag for all types of assets
/// used in main AssetProcessor (AssetProcessor.h)
class ReplaceRequest {
 public:
  ReplaceRequest() = default;

  ReplaceRequest(const ReplaceRequest&) = default;
  ReplaceRequest& operator=(const ReplaceRequest&) = default;

  ReplaceRequest(ReplaceRequest&&) = default;
  ReplaceRequest& operator=(ReplaceRequest&&) = default;

  void ClearFlags() {
    force_true_ = false;
    force_false_ = false;
  }

  bool operator()(std::string question) {
    if (force_true_) {
      std::cout << question << " -auto yes-" << std::endl;
      return true;
    } else if (force_false_) {
      std::cout << question << " -auto no-" << std::endl;
      return false;
    }
    std::cout << question
              << "\ny(yes), n(no), a(yes for all), 0(no for all))"
              << std::endl;
    bool answer = false; // "NO" by default
    char response;
    std::cin >> response;
    if (response == 'y' || response == 'Y') {
      answer = true;
    } else if (response == 'a' || response == 'A') {
      force_true_ = true;
      answer = true;
    } else if (response == '0') {
      force_false_ = true;
    }
    if (answer) {
      std::cout << "Chosen: YES" << std::endl;
    } else {
      std::cout << "Chosen: NO" << std::endl;
    }
    return answer;
  }

 private:
  bool force_true_{false};
  bool force_false_{false};
};

#endif  // FAITHFUL_UTILS_ASSETPROCESSOR_REPLACEREQUEST_H_
