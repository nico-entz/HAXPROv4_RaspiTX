latlon=$(gpspipe -w -n 200 | grep -m 1 TPV)
echo "$latlon" | jq -r '"\(.time),\(.lat),\(.lon),\(.alt)"'
