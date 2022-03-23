sockets="$(lscpu | grep 'Socket(s):' | awk '{print $2}')"
persockcores="$(lscpu | grep 'Core(s) per socket' | awk '{print $4}')"

declare -A socketsA

while read -r line;
do
    s="$(echo "$line" | awk '{print $2}')"
    c="$(echo "$line" | awk '{print $1}')"
    if [ "${socketsA["$s"]}" ]; then
        x=$(echo -e "${socketsA["$s"]}\n$c" | sort | uniq)
        socketsA["$s"]="$x"
    else
        socketsA["$s"]="$c"
    fi
done < <(lscpu -e='core,node' | tail -n +2)

echo "/*"
echo " * The code is a part of our project called VEGITO, which retrofits"
echo " * high availability mechanism to tame hybrid transaction/analytical"
echo " * processing."
echo " *"
echo " * Copyright (c) 2021 Shanghai Jiao Tong University."
echo " *     All rights reserved."
echo " *"
echo " *  Licensed under the Apache License, Version 2.0 (the "License");"
echo " *  you may not use this file except in compliance with the License."
echo " *  You may obtain a copy of the License at"
echo " *"
echo " *      http://www.apache.org/licenses/LICENSE-2.0"
echo " *"
echo " *  Unless required by applicable law or agreed to in writing,"
echo " *  software distributed under the License is distributed on an \"AS"
echo " *  IS\" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either"
echo " *  express or implied.  See the License for the specific language"
echo " *  governing permissions and limitations under the License."
echo " *"
echo " * For more about this software visit:"
echo " *"
echo " *      http://ipads.se.sjtu.edu.cn/projects/vegito"
echo " *"
echo " */"
echo ""
echo "#pragma once"
echo ""
echo "// you can use \`lscpu\` to check the CPU information"
echo ""
echo "// Core(s) per socket"
echo "#define PER_SOCKET_CORES  $persockcores    // number of (physical) cores per socket"
echo ""
echo "// Socket(s)"
echo "#define NUM_SOCKETS       $sockets     // number of sockets"
echo ""
echo "// NUMA node0 CPU(s)"

for ((i = 0; i < "$sockets"; i++)); do
    echo "#define SOCKET_$i \\"
    l=$(echo -en "${socketsA[$i]}" | tr '\n' ',')
    echo "  $l"
    echo ""
done


echo "#define CPUS \\"
echo "  { \\"
for ((i = 0; i < "$sockets"-1; i++)); do
    echo "  {SOCKET_$i}, \\"
done
end=$(("$sockets"-1))
echo "  {SOCKET_$end} \\"
echo "  }"
echo ""
echo "// choose RDMA NIC id according to sockets"
echo "// you can use 0 if you only have one NIC"
echo "#define CHOOSE_NIC(core_id) \\"
echo "  ((core_id > PER_SOCKET_CORES)? 0 : 1)"
