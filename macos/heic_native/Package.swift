// swift-tools-version: 5.9

import PackageDescription

let package = Package(
    name: "heic_native",
    platforms: [
        .macOS("10.14")
    ],
    products: [
        .library(name: "heic_native", targets: ["heic_native"])
    ],
    dependencies: [],
    targets: [
        .target(
            name: "heic_native",
            dependencies: [],
            resources: [
                .process("PrivacyInfo.xcprivacy"),
            ]
        )
    ]
)
