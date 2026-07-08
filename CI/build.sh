#!/bin/bash
set -e

IsWindows=false
IsMacOS=false
IsLinux=false
is64BitOperatingSystem=false
CurrentDir=$(dirname "$0")

cmakeInstallPrefix='out/install'
ArtifactPath=''
IncludeDebug=false
Version=''

# check platform
unameOut="$(uname -s)"
case "${unameOut}" in
    Linux*)     IsLinux=true;;
    Darwin*)    IsMacOS=true;;
    CYGWIN*)    IsWindows=true;;
    MINGW*)     IsWindows=true;;
    *)          ;;
esac

if [ "$(uname -m)" == "x86_64" ]; then
    is64BitOperatingSystem=true
fi

parseArgs() {
    args=("$@")
    for ((i=0; i<${#args[@]}; i++)); do
        case ${args[i]} in
            '-ArtifactPath')
                ArtifactPath=${args[++i]}
                ;;
            '-IncludeDebug')
                IncludeDebug=true
                ;;
            '-Version')
                Version=${args[++i]}
                ;;
            *)
                ;;
        esac
    done
}

printEnvironments() {
    cat << EOF
IsWindows: $IsWindows
IsMacOS: $IsMacOS
IsLinux: $IsLinux
Is64BitOperatingSystem: $is64BitOperatingSystem
Current working directory: $(pwd)
ArtifactPath: $ArtifactPath
IncludeDebug: $IncludeDebug
Version: $Version
EOF
}

installVcpkg() {
    vcpkgUrl='https://github.com/microsoft/vcpkg.git'
    git clone "$vcpkgUrl"
    
    if [ "$IsWindows" = true ]; then
        ./vcpkg/bootstrap-vcpkg.bat
    elif [ "$IsMacOS" = true ] || [ "$IsLinux" = true ]; then
        ./vcpkg/bootstrap-vcpkg.sh
        if [ "$IsMacOS" = true ]; then
            # xcode-select --install
            :
        fi
    else
        echo 'vcpkg is not available on target platform.'
        exit 1
    fi
}

getVcpkgTriplet() {
    if [ "$IsWindows" = true ]; then
        echo "x64-windows"
    elif [ "$IsLinux" = true ]; then
        echo "x64-linux"
    else
        echo ""
    fi
}

downloadFile() {
    url="$1"
    dest="$2"
    unixDest=${dest//\\//}

    if [ -z "$url" ]; then
        echo "Missing download URL for $unixDest."
        exit 1
    fi

    file=$(basename "$unixDest")
    dir=$(dirname "$unixDest")
    tmp="$dir/$file.download"
    mkdir -p "$dir"

    curl --fail --location --retry 3 --retry-delay 5 \
        --user-agent "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36" \
        -o "$tmp" "$url"

    if [ ! -s "$tmp" ]; then
        echo "Downloaded file is empty: $url"
        exit 1
    fi

    mv "$tmp" "$unixDest"
}

validateFbxSdkHome() {
    home="$1"
    if [ ! -f "$home/include/fbxsdk.h" ]; then
        echo "FBX SDK installation is invalid: $home/include/fbxsdk.h is missing."
        find "$(dirname "$home")" -maxdepth 3 -type f -name 'fbxsdk.h' -print || true
        exit 1
    fi
}

patchFbxSdkHeaders() {
    home="$1"
    redBlackTreeHeader="$home/include/fbxsdk/core/base/fbxredblacktree.h"
    if [ -f "$redBlackTreeHeader" ] && grep -q 'mLefttChild' "$redBlackTreeHeader"; then
        echo "Patching FBX SDK header typo: $redBlackTreeHeader"
        if [ "$IsMacOS" = true ]; then
            sudo sed -i '' 's/mLefttChild/mLeftChild/g' "$redBlackTreeHeader"
        else
            sed -i 's/mLefttChild/mLeftChild/g' "$redBlackTreeHeader"
        fi
    fi
}

installFbxSdk() {
    fbxSdkHome=$(pwd)/fbxsdk/Home
    fbxSdkValidateHome=$fbxSdkHome
    mkdir -p fbxsdk

    if [ "$IsWindows" = true ]; then
        fbxSdkUrl="https://damassets.autodesk.net/content/dam/autodesk/www/adn/fbx/2020-3-4/fbx202034_fbxsdk_vs2022_win.exe"
        fbxSdkWindowsInstaller="fbxsdk/fbxsdk.exe"
        fbxSdkWindowsInstallerForCmd="fbxsdk\\fbxsdk.exe"

        downloadFile "$fbxSdkUrl" "$fbxSdkWindowsInstaller"
        
        fbxSdkHome_unix=$fbxSdkHome
        fbxSdkValidateHome=$fbxSdkHome_unix
        echo fbxSdkHome_unix=$fbxSdkHome_unix

        fbxSdkHome=$(cygpath -w $fbxSdkHome_unix)

        echo fbxSdkHome=$fbxSdkHome

        cmd "/C CI\install-fbx-sdk.bat $fbxSdkWindowsInstallerForCmd $fbxSdkHome"
        echo "Installation finished($fbxSdkHome)."

    elif [ "$IsMacOS" = true ]; then
        fbxSdkUrl="https://damassets.autodesk.net/content/dam/autodesk/www/adn/fbx/2020-3-4/fbx202034_fbxsdk_clang_mac.pkg.tgz"
        fbxSdkMacOSTarball='./fbxsdk/fbxsdk.pkg.tgz'

        downloadFile "$fbxSdkUrl" "$fbxSdkMacOSTarball"

        tar -zxvf "$fbxSdkMacOSTarball" -C fbxsdk
        fbxSdkMacOSPkgFile=$(find fbxsdk -name '*.pkg' -type f)
        echo "FBX SDK MacOS pkg: $fbxSdkMacOSPkgFile"
        sudo installer -pkg "$fbxSdkMacOSPkgFile" -target /
        fbxSdkInstalledDir=$(find "/Applications/Autodesk/FBX SDK" -mindepth 1 -maxdepth 1 -type d | sort | tail -n 1)
        if [ -z "$fbxSdkInstalledDir" ]; then
            echo "FBX SDK install directory was not found under /Applications/Autodesk/FBX SDK."
            exit 1
        fi
        ln -s "$fbxSdkInstalledDir" fbxsdk/Home
    elif [ "$IsLinux" = true ]; then
        fbxSdkUrl="https://damassets.autodesk.net/content/dam/autodesk/www/adn/fbx/2020-3-4/fbx202034_fbxsdk_linux.tar.gz"
        fbxSdkTarball=fbxsdk/fbxsdk.tar.gz

        echo "Downloading FBX SDK tar ball from $fbxSdkUrl ..."
        downloadFile "$fbxSdkUrl" "$fbxSdkTarball"
        tar -zxvf "$fbxSdkTarball" -C fbxsdk

        fbxSdkInstallationProgram=$(find fbxsdk -maxdepth 2 -type f -name 'fbx*_fbxsdk_linux' | head -n 1)
        if [ -z "$fbxSdkInstallationProgram" ]; then
            echo "FBX SDK Linux installer was not found in $fbxSdkTarball."
            exit 1
        fi
        chmod ugo+x "$fbxSdkInstallationProgram"

        fbxSdkHomeLocation="$HOME/fbxsdk/install"
        echo "Installing from $fbxSdkInstallationProgram..."
        mkdir -p "$fbxSdkHomeLocation"

        # This is really a HACK way after many tries...
        yes yes | "$fbxSdkInstallationProgram" "$fbxSdkHomeLocation"
        echo ''

        fbxSdkHome="$fbxSdkHomeLocation"
        fbxSdkValidateHome="$fbxSdkHomeLocation"
        echo "Installation finished($fbxSdkHomeLocation)."
    else
        echo 'FBXSDK is not available on target platform.'
        exit 1
    fi

    validateFbxSdkHome "$fbxSdkValidateHome"
    patchFbxSdkHeaders "$fbxSdkValidateHome"
    echo "$fbxSdkHome"
}

installDependenciesForMacOS() {
    # Download both x86-64 and arm-64 libs and merge them into a uniform binary.
    # https://www.f-ax.de/dev/2022/11/09/how-to-use-vcpkg-with-universal-binaries-on-macos/
    dependencies=('libxml2' 'zlib' 'fmt' 'nlohmann-json' 'glm' 'cppcodec' 'range-v3' 'cxxopts' 'doctest' 'utfcpp')
    for libName in "${dependencies[@]}"; do
        ./vcpkg/vcpkg install --triplet=x64-osx "$libName"
        ./vcpkg/vcpkg install --triplet=arm64-osx "$libName"
    done

    python3 ./CI/lipo-dir-merge.py ./vcpkg/installed/arm64-osx ./vcpkg/installed/x64-osx ./vcpkg/installed/uni-osx
}

installDependenciesForOthers() {
    dependencies=('libxml2' 'zlib' 'fmt' 'nlohmann-json' 'glm' 'cppcodec' 'range-v3' 'cxxopts' 'doctest' 'utfcpp')
    vcpkgTriplet=$(getVcpkgTriplet)
    for libName in "${dependencies[@]}"; do
        if [ -n "$vcpkgTriplet" ]; then
            ./vcpkg/vcpkg install --triplet="$vcpkgTriplet" "$libName"
        else
            ./vcpkg/vcpkg install "$libName"
        fi
    done
}

installDependencies() {
    if [ "$IsMacOS" = true ]; then
        installDependenciesForMacOS
    else
        installDependenciesForOthers
    fi
}

runCMake() {
    buildType="$1"
    echo "Build $buildType ..."
    cmakeBuildDir="out/build/$buildType"

    polyfillsStdFileSystem='OFF'
    if [ "$IsWindows" = false ]; then
        polyfillsStdFileSystem='ON'
    fi

    defineVersion=''
    if [ -n "$Version" ]; then
        defineVersion="-DFBX_GLTF_CONV_CLI_VERSION=$Version"
    fi

    if [ "$IsMacOS" = true ]; then
        echo "fbx home is $fbxSdkHome"
        cmake -DCMAKE_TOOLCHAIN_FILE="vcpkg/scripts/buildsystems/vcpkg.cmake" \
            -DCMAKE_PREFIX_PATH="./vcpkg/installed/uni-osx" \
            -DVCPKG_TARGET_TRIPLET="uni-osx" \
            -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
            -DCMAKE_BUILD_TYPE="${buildType}" \
            -DCMAKE_INSTALL_PREFIX="${cmakeInstallPrefix}/${buildType}" \
            -DFbxSdkHome:STRING="${fbxSdkHome}" \
            -DPOLYFILLS_STD_FILESYSTEM="${polyfillsStdFileSystem}" \
            "${defineVersion}" \
            -S. -B"${cmakeBuildDir}"
    else
        vcpkgTriplet=$(getVcpkgTriplet)
        cmake -DCMAKE_TOOLCHAIN_FILE="vcpkg/scripts/buildsystems/vcpkg.cmake" \
                -DVCPKG_TARGET_TRIPLET="${vcpkgTriplet}" \
                -DCMAKE_BUILD_TYPE="${buildType}" \
                -DCMAKE_INSTALL_PREFIX="${cmakeInstallPrefix}/${buildType}" \
                -DFbxSdkHome:STRING="${fbxSdkHome}" \
                -DPOLYFILLS_STD_FILESYSTEM="${polyfillsStdFileSystem}" \
                "${defineVersion}" \
                -S. -B"${cmakeBuildDir}"
    fi

    cmake --build $cmakeBuildDir --config $buildType

    if [ "$IsWindows" = true ]; then
        cmake --build $cmakeBuildDir --config $buildType --target install
    else
        cmake --install $cmakeBuildDir
    fi
}

build() {
    cmakeBuildTypes=('Release')
    if [ "$IncludeDebug" = true ]; then
        cmakeBuildTypes+=('Debug')
    fi

    for buildType in "${cmakeBuildTypes[@]}"; do
        runCMake "$buildType"
    done

    if [ ! -d "$cmakeInstallPrefix" ] || [ ! -e "$cmakeInstallPrefix" ]; then
        echo 'Installation failed.'
        exit -1
    fi
    
    if [ -n "$ArtifactPath" ]; then
        tar -czvf $ArtifactPath -C $cmakeInstallPrefix .
    fi
}

parseArgs "$@"
printEnvironments
installFbxSdk
installVcpkg
installDependencies
build
