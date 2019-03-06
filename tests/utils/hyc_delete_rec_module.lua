local function split(str, sep)
	local result = {}
	local regex = ("([^%s]+)"):format(sep)
	for each in str:gmatch(regex) do
		table.insert(result, each)
	end

	return result
end

function hyc_delete_rec(rec, input_ids)
	trace("HYC_UDF args:: (%s)--------------------", tostring(input_ids));
	trace("HYC_UDF key:: (%s)--------------------", record.key(rec));

	local rec_vmdkid = -1; rec_ckptid = -1; rec_offset = -1
	local count = 0
	local vals = split(record.key(rec), ":")
	for _,val in ipairs(vals) do
		if count == 0 then
			rec_vmdkid = tonumber(val)
		elseif count == 1 then
			rec_ckptid = tonumber(val)
		elseif count == 2 then
			rec_offset = tonumber(val)
		end

		count = count + 1
	end

	trace("HYC_UDF Post split count : %d, vmdkid : %d, ckpt_id : %d, offset :%d", count, rec_vmdkid, rec_ckptid, rec_offset);
	if vmdkid == -1 or ckpt_id == -1 or offset == -1 or count ~= 3 then
		trace("HYC_UDF Error, Invalid record format");
	else
		local id
		for id in list.iterator(input_ids) do
			id = tonumber(id)
			if rec_vmdkid == id then
				trace("HYC_UDF found match with :: %d", tonumber(id));
				aerospike:remove(rec)
			end
		end
	end
end
