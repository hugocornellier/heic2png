import Cocoa
import FlutterMacOS
import XCTest

@testable import heic_native

class RunnerTests: XCTestCase {

  func testUnknownMethodReturnsNotImplemented() {
    let plugin = HeicNativePlugin()

    let call = FlutterMethodCall(methodName: "unknownMethod", arguments: [])

    let resultExpectation = expectation(description: "result block must be called.")
    plugin.handle(call) { result in
      XCTAssertEqual(result as? NSObject, FlutterMethodNotImplemented)
      resultExpectation.fulfill()
    }
    waitForExpectations(timeout: 1)
  }

}
